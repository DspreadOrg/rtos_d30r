/**********************************************************************************************************
 * File Name     : page_scan.c
 * Description   : Scan page (camera preview + result display)
 *
 *   The camera preview is output directly to the screen by Lib_CamDisp(TRUE); LVGL only handles prompts and result text.
 *   The scan process polls with lv_timer to avoid blocking the UI thread.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_scan.h"
#include "app_ui.h"

#include <stdio.h>
#include <string.h>

#define SCAN_POLL_MS        40
#define SCAN_BUF_LEN        256

static lv_obj_t *s_statusLabel = NULL;
static lv_obj_t *s_resultLabel = NULL;
static lv_obj_t *s_rescanButton = NULL;
static lv_timer_t *s_timer = NULL;
static bool s_done = false;

static void scan_refresh_status(const char *text, lv_color_t color)
{
    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR(text));
        lv_obj_set_style_text_color(s_statusLabel, color, 0);
    }
}

static void scan_begin(void)
{
    s_done = false;
    if (s_resultLabel != NULL)
    {
        lv_label_set_text(s_resultLabel, "-");
    }

    if (app_scan_start() != RET_OK)
    {
        scan_refresh_status("Camera open failed", APP_COLOR_WARN);
        s_done = true;
        return;
    }

    scan_refresh_status("Scanning...", APP_COLOR_ACCENT);
}

static void scan_rescan_cb(lv_event_t *e)
{
    (void)e;

    scan_begin();
}

static void scan_timer_cb(lv_timer_t *timer)
{
    unsigned char buf[SCAN_BUF_LEN];
    unsigned short len = sizeof(buf);
    int rc;

    (void)timer;

    if (s_done)
    {
        return;
    }

    rc = app_scan_poll(buf, &len);
    if (rc != 1)
    {
        return;
    }

    buf[len] = 0;
    s_done = true;
    app_scan_stop();

    if (s_resultLabel != NULL)
    {
        lv_label_set_text(s_resultLabel, (char *)buf);
    }
    scan_refresh_status("Scanned", APP_COLOR_SUCCESS);
}

static void scan_create(app_page_t *page)
{
    lv_obj_t *tip = NULL;
    lv_obj_t *btn = NULL;
    lv_obj_t *btnLabel = NULL;

    s_statusLabel = app_row(page->body, APP_ROW_START_Y, "Status");
    s_resultLabel = app_row(page->body, APP_ROW_START_Y + APP_ROW_H + APP_ROW_GAP, "Result");
    if (s_resultLabel != NULL)
    {
        lv_label_set_long_mode(s_resultLabel, LV_LABEL_LONG_WRAP);
    }

    /* On-screen rescan button so the physical ENTER key is not the only option */
    btn = lv_obj_create(page->body);
    if (btn != NULL)
    {
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, APP_ROW_X, APP_ROW_START_Y + 168);
        lv_obj_set_size(btn, APP_ROW_W, 72);
        lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        btnLabel = lv_label_create(btn);
        lv_label_set_text(btnLabel, LIB_TR("Rescan"));
        lv_obj_set_style_text_color(btnLabel, lv_color_white(), 0);
        lv_obj_set_style_text_font(btnLabel, app_font(20), 0);
        lv_obj_center(btnLabel);

        lv_obj_add_event_cb(btn, scan_rescan_cb, LV_EVENT_CLICKED, NULL);
    }
    s_rescanButton = btn;

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Tap Rescan for another scan, bottom BACK to return"));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(14), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_timer = lv_timer_create(scan_timer_cb, SCAN_POLL_MS, NULL);
    scan_begin();
}

static void scan_destroy(app_page_t *page)
{
    (void)page;

    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    app_scan_stop();
    s_statusLabel = NULL;
    s_resultLabel = NULL;
    s_rescanButton = NULL;
    s_done = true;
}

static bool scan_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param == KB_ENTER && s_done)
    {
        scan_begin();
        return true;
    }

    return false;
}

static app_page_t s_scanPage =
{
    .id         = "scan",
    .title      = "SCAN",
    .on_create  = scan_create,
    .on_event   = scan_event,
    .on_destroy = scan_destroy,
    .user_data  = NULL,
};

app_page_t *page_scan_get(void)
{
    return &s_scanPage;
}
