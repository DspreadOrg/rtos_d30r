/**********************************************************************************************************
 * File Name     : page_tms.c
 * Description   : TMS remote update page (UI layer)
 *
 *   Handles UI and interaction only: select a module -> call app_tms_download() -> show the result.
 *   The download protocol, resume support, verification and parameter write-back all live in libTms; no parsing is done here.
 *
 *   Key improvements over the previous version:
 *     - The download runs in a separate thread tms_worker, so the UI main loop keeps running;
 *       touch and keys still respond during the download, and the nav-bar back key can cancel at any time;
 *     - libTms event/progress callbacks run in the download thread and only post
 *       APP_EV_TMS_EVENT / APP_EV_TMS_PROGRESS / APP_EV_TMS_DONE to the event queue,
 *       which on_event in this page consumes in the UI thread to refresh the status card.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_tms.h"
#include "app_ui.h"
#include "LibApi.h"
#include "LibOsi.h"

#include <stdio.h>
#include <string.h>

#define TMS_ITEM_COUNT      5
#define TMS_ROW_Y           52
#define TMS_ROW_H           76
#define TMS_ROW_GAP         10
#define TMS_STATUS_Y        492
#define TMS_STATUS_H        150
#define TMS_TIMEOUT_MS      (60 * 1000)     /* handshake timeout, consistent with the 01SDK_DEMO menu */

typedef struct
{
    const char *title;
    TmsModule module;
} tms_item_t;

static const tms_item_t s_items[TMS_ITEM_COUNT] =
{
    { "1  OS / GPRS Module", TMS_MODULE_GPRS     },
    { "2  App Firmware",     TMS_MODULE_FIRMWARE },
    { "3  File System",      TMS_MODULE_FS       },
    { "4  WiFi Module",      TMS_MODULE_WIFI     },
    { "5  SE Module",        TMS_MODULE_SE       },
};

static lv_obj_t *s_statusLabel = NULL;
static volatile unsigned char s_running = 0;    /* UI thread writes 1, download thread writes 0 when finished */

/* Background download thread: created on first download start, then stays resident waiting on the semaphore */
static void          *s_tmsSem    = NULL;
static void          *s_tmsThread = NULL;
static TmsModule    s_tmsModule = TMS_MODULE_GPRS;
static int            s_curIndex  = 0;

/* ---------------- Status card text updates ---------------- */

static void tms_set_status(const char *text, lv_color_t color)
{
    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, text ? text : "");
        lv_obj_set_style_text_color(s_statusLabel, color, 0);
    }
}

/* Status text for each libTms event */
static const char *tms_event_text(TmsEventId event)
{
    switch (event)
    {
    case TMS_EVT_CONNECTING:    return "Connecting to TMS server...";
    case TMS_EVT_TASK_QUERY:    return "Querying task...";
    case TMS_EVT_NO_TASK:       return "No download task";
    case TMS_EVT_IS_LATEST:     return "Already the latest version";
    case TMS_EVT_NO_MATCH:      return "No matching firmware";
    case TMS_EVT_DOWNLOADING:   return "Downloading...";
    case TMS_EVT_DOWNLOAD_DONE: return "Download finished";
    case TMS_EVT_RETRYING:      return "Retrying...";
    case TMS_EVT_VERIFYING:     return "Verifying / installing...";
    case TMS_EVT_VERIFY_FAIL:   return "Verify failed";
    case TMS_EVT_INSTALL_FAIL:  return "Install failed";
    case TMS_EVT_SUCCESS:       return "Upgrade succeeded, rebooting...";
    case TMS_EVT_CANCELLED:     return "Cancelled";
    case TMS_EVT_NET_ERROR:     return "Network error";
    case TMS_EVT_DATA_ERROR:    return "Data error";
    case TMS_EVT_SERVER_REPLY:  return "Server rejected the request";
    default:                    return NULL;
    }
}

/* ---------------- UI thread: consume TMS events and refresh the status card ---------------- */
static char *tms_resp_code(int param)
{
    switch (param)
    {
    case 0x00://00  Success
        return "00 Success";

    case 0x01://01 Unknown terminal No.
        return "01 Unknown term No.";

    case 0x02://02 Software is the latest, no update needed
        return "02 Latest version";

    case 0x03://03 Parameters are the latest, no update needed
        return "03 Latest version params";

    case 0x04://04 Server refuses update
        return "04 Server refuse update";

    case 0x05://05 System busy
        return "05 System busy";
        break;

    case 0x06://06 Download version not found
        return "06 Download ver not find, Server No latest version";

    case 0x07://07 Unknown notice type
        return "07 Unknown notice type";

    case 0x08://08 Lrc check error
        return "08 Lrc check error";

    case 0x09://09 No update required
        return "09 No update required";

    case 0x10://10 Requested version mismatch, downloading again
        return "10 Request ver not same";

    default://other errors
        return "Other error";

    }
}

/* Flow event -> status text */
static void tms_show_event(TmsEventId event, int param)
{
    char text[96] = { 0 };
    const char *base = NULL;

    if (!s_statusLabel)
    {
        return;
    }

    base = tms_event_text(event);

    if (event == TMS_EVT_RETRYING)
    {
        snprintf(text, sizeof(text), "Retrying (%d)...", param);
    }
    else if (event == TMS_EVT_SERVER_REPLY)
    {
        snprintf(text, sizeof(text), "Server reply code\n%s", tms_resp_code(param));
    }
    else
    {
        snprintf(text, sizeof(text), "%s", base ? base : "");
    }

    tms_set_status(text, (event == TMS_EVT_SUCCESS) ? APP_COLOR_SUCCESS : APP_COLOR_TEXT);
}

/* Download progress -> status text */
static void tms_show_progress(int percent)
{
    char text[96] = { 0 };

    if (!s_statusLabel)
    {
        return;
    }

    snprintf(text, sizeof(text), "Downloading %d%%", percent);
    tms_set_status(text, APP_COLOR_ACCENT);
}

/* Download finished -> final result */
static void tms_show_done(int rc)
{
    char text[96] = { 0 };

    if (!s_statusLabel)
    {
        return;
    }

    if (rc == TMS_OK)
    {
        Lib_Beep(BEEP_TIME_DEF);
        snprintf(text, sizeof(text), "%s\nUpgrade finished", s_items[s_curIndex].title + 3);
        tms_set_status(text, APP_COLOR_SUCCESS);
    }
    else if (rc == TMS_ERR_CANCEL)
    {
        snprintf(text, sizeof(text), "%s\nCancelled", s_items[s_curIndex].title + 3);
        tms_set_status(text, APP_COLOR_TEXT);
    }
    else if (rc == WLS_NOSIM)
    {
        snprintf(text, sizeof(text), "%s\nNo SIM card", s_items[s_curIndex].title + 3);
        tms_set_status(text, APP_COLOR_WARN);
    }
    else
    {
        //snprintf(text, sizeof(text), "%s\nFailed, code %d", s_items[s_curIndex].title + 3, rc);
        //tms_set_status(text, APP_COLOR_WARN);
    }
}

/* ---------------- Download thread ---------------- */

/* libTms event callback (runs in the download thread): only posts events, never touches any LVGL widget.
   param packing: low 16 bits = TmsEventId, high 16 bits = retry count / server reply code, etc. */
static void tms_on_event(TmsEventId event, int param)
{
    app_event_post_simple(APP_EV_TMS_EVENT,
                          (int32_t)((event & 0xFFFF) | ((param & 0xFFFF) << 16)));
}

/* libTms progress callback (runs in the download thread, already rate-limited to 10Hz internally): only posts the percentage */
static void tms_on_progress(TmsModule module, unsigned long done, unsigned long total)
{
    (void)module;

    if (total == 0)
    {
        return;
    }

    app_event_post_simple(APP_EV_TMS_PROGRESS, (int32_t)((done * 100ULL) / total));
}

/* Resident download thread: wait for the start semaphore -> blocking download -> notify the UI thread -> loop */
static void tms_worker(void *arg)
{
    (void)arg;

    while (1)
    {
        short rc;

        Lib_osiSemaphoreAcquire(s_tmsSem);

        rc = app_tms_download(s_tmsModule, TMS_TIMEOUT_MS);

        s_running = 0;

        /* This page may no longer be in the foreground when the download ends; the current page ignores the event itself */
        app_event_post_simple(APP_EV_TMS_DONE, rc);
    }
}

/* Called from the UI thread: trigger one background download (ignored if already downloading) */
static void tms_start(int index)
{
    if ((s_running) || (index < 0) || (index >= TMS_ITEM_COUNT))
    {
        return;
    }

    if (s_tmsSem == NULL)
    {
        s_tmsSem = Lib_osiSemaphoreCreate(1, 0);
    }

    if ((s_tmsSem != NULL) && (s_tmsThread == NULL))
    {
        s_tmsThread = Lib_osiThreadCreate("tmsWorker", tms_worker, NULL,
                                           OSI_PRIORITY_NORMAL,
                                           (unsigned long)32 * 1024, 0);
    }

    if ((s_tmsSem == NULL) || (s_tmsThread == NULL))
    {
        tms_set_status("Failed to start download task", APP_COLOR_WARN);
        return;
    }

    s_curIndex  = index;
    s_tmsModule = s_items[index].module;
    s_running   = 1;

    tms_set_status("Connecting to TMS server...", APP_COLOR_ACCENT);

    Lib_osiSemaphoreRelease(s_tmsSem);
}

static void tms_row_cb(lv_event_t *e)
{
    tms_start((int)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *tms_row(lv_obj_t *parent, lv_coord_t y, const char *title, int index)
{
    lv_obj_t *row = app_card(parent, APP_ROW_X, y, APP_ROW_W, TMS_ROW_H);
    lv_obj_t *label = NULL;
    lv_obj_t *arrow = NULL;

    if (row == NULL)
    {
        return NULL;
    }

    label = lv_label_create(row);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);

    arrow = lv_label_create(row);
    lv_label_set_text(arrow, ">");
    lv_obj_set_style_text_color(arrow, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(arrow, app_font(20), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_add_event_cb(row, tms_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return row;
}

/* ---------------- Page lifecycle ---------------- */

static void tms_create(app_page_t *page)
{
    lv_obj_t *tip = NULL;
    lv_obj_t *card = NULL;

    /* Note: do not clear s_running -- if the page is left and re-entered during a download,
       the background thread is still running and the state must be preserved */
    s_statusLabel = NULL;

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, "Select the module to update (TMS server + network required)");
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(16), 0);
    lv_obj_set_pos(tip, APP_ROW_X, 16);

    for (int i = 0; i < TMS_ITEM_COUNT; i++)
    {
        tms_row(page->body, TMS_ROW_Y + i * (TMS_ROW_H + TMS_ROW_GAP), s_items[i].title, i);
    }

    card = app_card(page->body, APP_ROW_X, TMS_STATUS_Y, APP_ROW_W, TMS_STATUS_H);
    if (card != NULL)
    {
        lv_obj_t *title = lv_label_create(card);

        lv_label_set_text(title, "Status");
        lv_obj_set_style_text_color(title, APP_COLOR_SUB, 0);
        lv_obj_set_style_text_font(title, app_font(16), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 12);

        s_statusLabel = lv_label_create(card);
        lv_label_set_text(s_statusLabel, "Ready");
        lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_statusLabel, app_font(20), 0);
        lv_obj_set_pos(s_statusLabel, 16, 42);
        lv_obj_set_width(s_statusLabel, APP_ROW_W - 32);
        lv_label_set_long_mode(s_statusLabel, LV_LABEL_LONG_WRAP);
    }

    /* Register the event/progress callbacks (they run in the download thread and only post to the event
       queue, consumed by tms_event above in the UI thread to refresh the status card) */
    app_tms_set_callbacks(tms_on_event, tms_on_progress);
}

static void tms_destroy(app_page_t *page)
{
    (void)page;

    /* Only unregister callbacks and widget pointers; the download thread keeps running in the
       background and s_running keeps the real state (the nav bar uses it to decide whether to block the back key) */
    app_tms_set_callbacks(NULL, NULL);   /* unregister, so this page's widgets are not refreshed after leaving */
    s_statusLabel = NULL;
}

/* All event handling runs in the UI thread: keys trigger the download, queued events refresh the status card */
static bool tms_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    switch (ev->type)
    {
        case APP_EV_KEY:
            if ((ev->param >= KB_DIGIT_ONE) && (ev->param <= KB_DIGIT_FIVE))
            {
                tms_start((int)(ev->param - KB_DIGIT_ONE));
                return true;
            }
            return false;

        case APP_EV_TMS_EVENT:
            tms_show_event((TmsEventId)(ev->param & 0xFFFF),
                           (int)((ev->param >> 16) & 0xFFFF));
            return true;

        case APP_EV_TMS_PROGRESS:
            tms_show_progress((int)ev->param);
            return true;

        case APP_EV_TMS_DONE:
            tms_show_done((int)ev->param);
            return true;

        default:
            return false;
    }
}

static app_page_t s_tmsPage =
{
    .id         = "tms",
    .title      = "TMS UPDATE",
    .on_create  = tms_create,
    .on_event   = tms_event,
    .on_destroy = tms_destroy,
    .user_data  = NULL,
};

app_page_t *page_tms_get(void)
{
    return &s_tmsPage;
}

bool page_tms_is_running(void)
{
    return (s_running != 0);
}

void page_tms_cancel(void)
{
    /* Notify libTms to cancel: it returns immediately the next time it sends/receives data and detects the cancel flag */
    app_tms_request_cancel();
}
