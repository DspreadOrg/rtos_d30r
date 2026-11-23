/**********************************************************************************************************
 * File Name     : page_maintenance.c
 * Description   : Maintenance (download) mode page
 *
 *   Entering the page does not switch mode immediately; it starts only after tapping Start:
 *     - Before start, exit the page with the bottom nav back key / BACK key
 *     - After start, libSDK takes over the screen and serial download flow; the main menu is rebuilt on exit
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "app_event.h"
#include "LibSysapi.h"     /* Lib_MaintenanceMode() / Lib_GetDevState() */

#include <stdio.h>

/* Secure unlock: implemented inside the SDK, only a declaration is needed here */
extern int Lib_SecReset(void);

#define MAINT_ICON_SIZE         96
#define MAINT_BTN_W             360
#define MAINT_BTN_H             84

static lv_obj_t *s_status = NULL;
static lv_obj_t *s_tamperWarn = NULL;   /* Tamper warning label (shown when triggered) */
static lv_obj_t *s_unlockBtn = NULL;    /* Unlock button (shown when tamper triggered and device not sealed) */
static lv_obj_t *s_unlockLabel = NULL;  /* Text label on the unlock button */
static bool s_resetDone = false;        /* After Lib_SecReset() succeeds the button becomes a restart button */
static app_page_t *s_page = NULL;
static bool s_active = false;        /* Lib_MaintenanceMode() is running */
static bool s_exitRequested = false; /* Exit from maintenance mode requested */

/* True while maintenance mode runs: the nav bar back key then reboots the device (the SDK maintenance mode
   is a blocking loop with no external exit API, so reboot is the only way back to the normal system) */
bool page_maintenance_is_active(void)
{
    return s_active;
}

/* Request the SDK to exit maintenance mode. The actual page pop happens after Lib_MaintenanceMode() returns,
   avoiding deletion of in-use page widgets inside the SDK blocking loop */
void page_maintenance_exit(void)
{
    if (!s_active || s_exitRequested)
    {
        return;
    }

    s_exitRequested = true;
    Lib_SetKey(KB_CANCEL);
}

/* Run maintenance mode synchronously in the main loop. Must not be called directly in an LVGL callback;
   the SDK runs LVGL tasks again internally, and re-entering lv_timer_handler causes faults */
static void maint_run(void)
{
    if (s_active || s_page == NULL || app_page_current() != s_page)
    {
        return;
    }

    s_active = true;
    s_exitRequested = false;

    if (s_status != NULL)
    {
        lv_label_set_text(s_status, LIB_TR("Maintenance mode is running..."));
    }

    /* Show tamper warning on the maintenance screen when triggered (maintenance mode is a blocking loop, query once before entering) */
    if (s_tamperWarn != NULL)
    {
        unsigned char ucType = 0;

        if (page_tamper_status(&ucType))
        {
            char acBuf[64] = { 0 };

            snprintf(acBuf, sizeof(acBuf), "%s (0x%02X)",
                      LIB_TR("Tamper detected!"), ucType);
            lv_label_set_text(s_tamperWarn, acBuf);
            lv_obj_clear_flag(s_tamperWarn, LV_OBJ_FLAG_HIDDEN);

            /* Provide unlock button when tamper triggered and device not sealed (SE not sealed) */
            if (s_unlockBtn != NULL && Lib_GetDevState(ID_DEV_IS_SE_PROTECT) == 0)
            {
                lv_obj_clear_flag(s_unlockBtn, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(s_unlockBtn, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            lv_obj_add_flag(s_tamperWarn, LV_OBJ_FLAG_HIDDEN);

            if (s_unlockBtn != NULL)
            {
                lv_obj_add_flag(s_unlockBtn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    lv_refr_now(NULL);

    /* Take over SDK UI drawing and key polling during maintenance mode so the nav bar stays visible and clickable */
    Lib_MaintenanceMode();

    s_active = false;

    /* Clear the page stack after maintenance mode ends, returning to the main menu */
    if (app_page_current() == s_page)
    {
        app_page_pop_to_root();
    }

    s_page = NULL;
    s_status = NULL;
    s_tamperWarn = NULL;
    s_unlockBtn = NULL;
    s_unlockLabel = NULL;
    s_resetDone = false;
}

/* Unlock button click: call SDK secure unlock; on success (returns 0) the button becomes a restart button,
   tapping again reboots the device */
static void maint_unlock_cb(lv_event_t *e)
{
    (void)e;

    if (s_resetDone)
    {
        Lib_Reboot();
        return;
    }

    if (Lib_SecReset() == 0)
    {
        s_resetDone = true;

        if (s_unlockLabel != NULL)
        {
            lv_label_set_text(s_unlockLabel, LIB_TR("Restart"));
        }
    }
}

static void maint_create(app_page_t *page)
{
    lv_obj_t *iconCircle = NULL;
    lv_obj_t *label = NULL;

    s_page = page;

    iconCircle = app_icon_circle(page->body, MAINT_ICON_SIZE);
    if (iconCircle != NULL)
    {
        lv_obj_align(iconCircle, LV_ALIGN_TOP_MID, 0, 60);
    }

    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Maintenance Mode"));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(28), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 190);

    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Device is now in firmware download mode."));
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(16), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 250);

    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Start the download tool on PC."));
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(16), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 278);

    s_status = lv_label_create(page->body);
    lv_label_set_text(s_status, LIB_TR("Starting maintenance mode..."));
    lv_obj_set_style_text_color(s_status, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_status, app_font(18), 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 350);

    /* Tamper warning label: hidden by default, shown as needed after query before entering maintenance mode */
    s_tamperWarn = lv_label_create(page->body);
    lv_label_set_text(s_tamperWarn, LIB_TR("Tamper detected!"));
    lv_obj_set_style_text_color(s_tamperWarn, APP_COLOR_WARN, 0);
    lv_obj_set_style_text_font(s_tamperWarn, app_font(22), 0);
    lv_obj_align(s_tamperWarn, LV_ALIGN_TOP_MID, 0, 410);
    lv_obj_add_flag(s_tamperWarn, LV_OBJ_FLAG_HIDDEN);

    /* Unlock button: hidden by default, shown when tamper triggered and device not sealed */
    s_unlockBtn = lv_obj_create(page->body);
    if (s_unlockBtn != NULL)
    {
        lv_obj_remove_style_all(s_unlockBtn);
        lv_obj_set_size(s_unlockBtn, 200, 64);
        lv_obj_set_style_radius(s_unlockBtn, 12, 0);
        lv_obj_set_style_bg_color(s_unlockBtn, APP_COLOR_WARN, 0);
        lv_obj_set_style_bg_opa(s_unlockBtn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_unlockBtn, lv_color_hex(0xBF360C), LV_STATE_PRESSED);
        lv_obj_add_flag(s_unlockBtn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_unlockBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_unlockBtn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_align(s_unlockBtn, LV_ALIGN_TOP_MID, 0, 480);
        lv_obj_add_flag(s_unlockBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(s_unlockBtn, maint_unlock_cb, LV_EVENT_CLICKED, NULL);

        label = lv_label_create(s_unlockBtn);
        lv_label_set_text(label, LIB_TR("Unlock"));
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, app_font(22), 0);
        lv_obj_center(label);
        s_unlockLabel = label;
    }

    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Tap BACK to exit maintenance mode"));
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(16), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -24);

    /* Page widgets created; post to main loop to start, avoiding entering the SDK blocking flow inside an LVGL callback */
    app_event_post_simple(APP_EV_MAINT_START, 0);
}

static void maint_destroy(app_page_t *page)
{
    (void)page;

    /* When the page is destroyed abnormally, ensure maintenance mode exit is requested */
    page_maintenance_exit();

    /* Must not zero page itself, app_page_pop() still uses page->root afterwards */
    s_page = NULL;
    s_status = NULL;
    s_tamperWarn = NULL;
    s_unlockBtn = NULL;
    s_unlockLabel = NULL;
    s_resetDone = false;
}

static bool maint_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type == APP_EV_MAINT_START)
    {
        maint_run();
        return true;
    }

    return false;
}

static app_page_t s_maintPage =
{
    .id         = "maintenance",
    .title      = "MAINTENANCE",
    .on_create  = maint_create,
    .on_event   = maint_event,
    .on_destroy = maint_destroy,
    .user_data  = NULL,
};

app_page_t *page_maintenance_get(void)
{
    return &s_maintPage;
}
