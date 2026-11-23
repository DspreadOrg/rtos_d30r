/**********************************************************************************************************
 * File Name     : ui_nav_bar.c
 * Description   : D30R bottom navigation bar (480 x 54)
 **********************************************************************************************************/

#include "ui_nav_bar.h"
#include "app_page.h"
#include "app_pages.h"
#include "app_ui.h"
#include "LibSysapi.h"     /* Lib_Reboot() */

#define NAV_BTN_COUNT       3
#define NAV_BTN_W           (APP_SCREEN_W / NAV_BTN_COUNT)   /* 160 */

/* Icon font used by the original navigation bar (defined in api_lvgl.c) */
extern lv_font_t custom_back_font;

extern void page_maintenance_exit(void);
/* Maintenance mode is a blocking SDK loop with no exit API: while it runs the nav key restarts the device,
   bringing the firmware back to the normal system; otherwise the normal page stack semantics apply */
static bool nav_exit_maintenance(void)
{
    if (!page_maintenance_is_active())
    {
        return false;
    }

    page_maintenance_exit();

    return true;
}

static void nav_menu_cb(lv_event_t *e)
{
    (void)e;

    if (nav_exit_maintenance())
    {
        return;
    }

    app_page_pop_to_root();
}

static void nav_home_cb(lv_event_t *e)
{
    (void)e;

    if (nav_exit_maintenance())
    {
        return;
    }

    app_page_pop_to_root();
}

static void nav_back_cb(lv_event_t *e)
{
    (void)e;

    if (nav_exit_maintenance())
    {
        return;
    }

    /* TMS page in the foreground and downloading: the back key semantics change to cancel the download;
       after leaving the TMS page the download moves to the background and back restores the page stack semantics */
    if (page_tms_is_running() && (app_page_current() == page_tms_get()))
    {
        page_tms_cancel();
        return;
    }

    app_page_pop();
}

static lv_obj_t *nav_create_button(lv_obj_t *parent, int index,
                                   const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, index * NAV_BTN_W, 0);
    lv_obj_set_size(btn, NAV_BTN_W, APP_NAV_BAR_H);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &custom_back_font, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

#define CUST_SYMBOL_MENU                "\xEE\x80\x80"
#define CUST_SYMBOL_HOME                "\xEE\x80\x81"
#define CUST_SYMBOL_BACK                "\xEE\x80\x82"
#define CUST_SYMBOL_POWER               "\xEE\x80\x83"
#define CUST_SYMBOL_UP                  "\xEE\x80\x84"
#define CUST_SYMBOL_DOWN                "\xEE\x80\x85"

void app_nav_bar_init(void)
{
    lv_obj_t *bar = lv_obj_create(lv_layer_top());

    if (bar == NULL)
    {
        return;
    }

    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), APP_NAV_BAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, APP_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    nav_create_button(bar, 0, CUST_SYMBOL_MENU, nav_menu_cb);
    nav_create_button(bar, 1, CUST_SYMBOL_HOME, nav_home_cb);
    nav_create_button(bar, 2, CUST_SYMBOL_BACK, nav_back_cb);
}
