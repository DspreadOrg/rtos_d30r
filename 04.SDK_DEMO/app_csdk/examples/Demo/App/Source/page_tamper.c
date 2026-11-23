/**********************************************************************************************************
 * File Name     : page_tamper.c
 * Description   : Tamper alert page + periodic check service
 *
 *   page_tamper_tick() is called by the main loop; the first query to Lib_TamperIsOccured() runs 10 seconds after boot,
 *   and then once every 60 seconds;
 *   when the state changes from "not triggered" to "triggered" (a new trigger), an APP_EV_TAMPER event is posted,
 *   and app_page_dispatch() pops up this page globally (ignored when already on this page).
 *   ENTER / OK / back key dismisses the alert; no repeated popup within the same trigger cycle.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibSysapi.h"

#include <stdio.h>

#define TAMPER_FIRST_CHECK_MS       (10UL * 1000UL)  /* First check after boot: 10 seconds */
#define TAMPER_CHECK_INTERVAL_MS    (60UL * 1000UL)  /* Check interval: 1 minute */

#define TAMPER_BANNER_Y             48
#define TAMPER_BANNER_H             240
#define TAMPER_ICON_SIZE            84
#define TAMPER_BTN_H                72
#define TAMPER_BTN_Y                488

static unsigned long s_dueAt = 0;        /* Next check due time (ms); 0 means not started yet */
static lv_obj_t *s_codeValue = NULL;     /* Tamper code value label */

bool page_tamper_status(unsigned char *pucType)
{
    unsigned char type = 0;

    if (Lib_TamperIsOccured(&type) != 0)
    {
        return false;
    }

    if (pucType != NULL)
    {
        *pucType = type;
    }

    return true;
}

void page_tamper_tick(void)
{
    unsigned long now = Lib_SysGetTimer();
    unsigned char type = 0;
    bool occurred;

    if (s_dueAt == 0)
    {
        /* First check 10 seconds after boot */
        s_dueAt = now + TAMPER_FIRST_CHECK_MS;
    }

    /* Skip if not due yet (half-period criterion, safe against 32-bit timer wrap-around) */
    if ((now - s_dueAt) > 0x7FFFFFFFUL)
    {
        return;
    }

    s_dueAt = now + TAMPER_CHECK_INTERVAL_MS;

    occurred = page_tamper_status(&type);

    if (occurred)
    {
        Lib_Trace("[TAMPER] occured type=0x%02X -> push tamper page\n", type);
        app_event_post_simple(APP_EV_TAMPER, (int32_t)type);
    }
}

static void tamper_ok_clicked_cb(lv_event_t *e)
{
    (void)e;

    app_page_pop();
}

static void tamper_create(app_page_t *page)
{
    unsigned char type = 0;
    char acBuf[32] = { 0 };
    lv_obj_t *banner = NULL;
    lv_obj_t *icon = NULL;
    lv_obj_t *label = NULL;
    lv_obj_t *btn = NULL;

    if (!page_tamper_status(&type))
    {
        type = 0;
    }

    /* Alert banner: warning-colored background + white exclamation mark circle */
    banner = lv_obj_create(page->body);
    if (banner != NULL)
    {
        lv_obj_remove_style_all(banner);
        lv_obj_set_pos(banner, APP_ROW_X, TAMPER_BANNER_Y);
        lv_obj_set_size(banner, APP_ROW_W, TAMPER_BANNER_H);
        lv_obj_set_style_radius(banner, 16, 0);
        lv_obj_set_style_bg_color(banner, APP_COLOR_WARN, 0);
        lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
        lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);

        icon = lv_obj_create(banner);
        lv_obj_remove_style_all(icon);
        lv_obj_set_size(icon, TAMPER_ICON_SIZE, TAMPER_ICON_SIZE);
        lv_obj_set_style_radius(icon, TAMPER_ICON_SIZE / 2, 0);
        lv_obj_set_style_bg_color(icon, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 28);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);

        label = lv_label_create(icon);
        lv_label_set_text(label, "!");
        lv_obj_set_style_text_color(label, APP_COLOR_WARN, 0);
        lv_obj_set_style_text_font(label, app_font(40), 0);
        lv_obj_center(label);

        label = lv_label_create(banner);
        lv_label_set_text(label, LIB_TR("Tamper detected!"));
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, app_font(32), 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -24);
    }

    /* Tamper code */
    s_codeValue = app_row(page->body,
                          TAMPER_BANNER_Y + TAMPER_BANNER_H + 24,
                          "Tamper Code");
    if (s_codeValue != NULL)
    {
        snprintf(acBuf, sizeof(acBuf), "0x%02X", type);
        lv_label_set_text(s_codeValue, acBuf);
    }

    /* Description */
    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Check the device enclosure for tampering."));
    lv_obj_set_width(label, APP_ROW_W);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(16), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, APP_ROW_X, TAMPER_BANNER_Y + TAMPER_BANNER_H + 116);

    /* OK button */
    btn = lv_obj_create(page->body);
    if (btn != NULL)
    {
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, APP_ROW_X, TAMPER_BTN_Y);
        lv_obj_set_size(btn, APP_ROW_W, TAMPER_BTN_H);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

        label = lv_label_create(btn);
        lv_label_set_text(label, LIB_TR("OK"));
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, app_font(22), 0);
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, tamper_ok_clicked_cb, LV_EVENT_CLICKED, NULL);
    }

    /* Bottom hint */
    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Press OK or BACK to dismiss"));
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(14), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -16);
}

static void tamper_destroy(app_page_t *page)
{
    (void)page;

    s_codeValue = NULL;
}

static bool tamper_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param == KB_ENTER)
    {
        app_page_pop();
        return true;
    }

    return false;
}

static app_page_t s_tamperPage =
{
    .id         = "tamper",
    .title      = "TAMPER ALERT",
    .on_create  = tamper_create,
    .on_event   = tamper_event,
    .on_destroy = tamper_destroy,
    .user_data  = NULL,
};

app_page_t *page_tamper_get(void)
{
    return &s_tamperPage;
}
