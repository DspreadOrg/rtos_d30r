/**********************************************************************************************************
 * File Name     : page_power.c
 * Description   : Power menu (power off / restart / cancel)
 *
 *   The power key is polled in app_power.c: a short press toggles screen off / on, and a long press of 1.5 seconds posts
 *   the KB_POWER_OFF event, which app_page_dispatch() uses to push this page uniformly.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibSysapi.h"     /* Lib_Reboot() / Lib_ShutDown() */
#include "LibLcd.h"        /* Lib_LcdClrScreen() */
#include "LibParamManage.h"

#define POWER_ITEM_COUNT        3
#define POWER_BTN_H             72
#define POWER_BTN_Y0            110
#define POWER_BTN_GAP           20

enum
{
    POWER_ITEM_OFF = 0,
    POWER_ITEM_RESTART,
    POWER_ITEM_CANCEL,
};

static const char *s_texts[POWER_ITEM_COUNT] =
{
    "Power Off", "Restart", "Cancel",
};

static lv_obj_t *s_buttons[POWER_ITEM_COUNT];
static int s_focus = 0;

static void power_apply_focus(void)
{
    int i;

    for (i = 0; i < POWER_ITEM_COUNT; i++)
    {
        if (s_buttons[i] == NULL)
        {
            continue;
        }

        if (i == s_focus)
        {
            lv_obj_set_style_border_width(s_buttons[i], 3, 0);
            lv_obj_set_style_border_color(s_buttons[i], APP_COLOR_TEXT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_buttons[i], 0, 0);
        }
    }
}

static void power_do(int index)
{
    switch (index)
    {
    case POWER_ITEM_OFF:
        Lib_ParamSave();

        Lib_LcdClrScreen();
        Lib_ShutDown();
        break;

    case POWER_ITEM_RESTART:
        Lib_ParamSave();

        Lib_Reboot();
        break;

    default:
        app_page_pop();
        break;
    }
}

static void power_clicked_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    power_do(index);
}

static lv_obj_t *power_create_button(lv_obj_t *parent, const char *text, lv_coord_t y,
                                     lv_color_t color, int index)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, APP_ROW_X, y);
    lv_obj_set_size(btn, APP_ROW_W, POWER_BTN_H);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(22), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, power_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return btn;
}

static void power_create(app_page_t *page)
{
    lv_obj_t *tip = NULL;
    int i;

    for (i = 0; i < POWER_ITEM_COUNT; i++)
    {
        lv_color_t color = APP_COLOR_ACCENT;

        if (i == POWER_ITEM_OFF)
        {
            color = APP_COLOR_WARN;
        }
        else if (i == POWER_ITEM_CANCEL)
        {
            color = APP_COLOR_SUB;
        }

        s_buttons[i] = power_create_button(page->body, LIB_TR(s_texts[i]),
                                           POWER_BTN_Y0 + i * (POWER_BTN_H + POWER_BTN_GAP),
                                           color, i);
    }

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Use UP / DOWN and ENTER, or tap an item"));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(14), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -16);

    s_focus = 0;
    power_apply_focus();
}

static void power_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < POWER_ITEM_COUNT; i++)
    {
        s_buttons[i] = NULL;
    }
}

static bool power_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    switch (ev->param)
    {
    case KB_UP:
        s_focus = (s_focus + POWER_ITEM_COUNT - 1) % POWER_ITEM_COUNT;
        power_apply_focus();
        return true;

    case KB_DOWN:
        s_focus = (s_focus + 1) % POWER_ITEM_COUNT;
        power_apply_focus();
        return true;

    case KB_ENTER:
        power_do(s_focus);
        return true;

    default:
        break;
    }

    return false;
}

static app_page_t s_powerPage =
{
    .id         = "power",
    .title      = "POWER",
    .on_create  = power_create,
    .on_event   = power_event,
    .on_destroy = power_destroy,
    .user_data  = NULL,
};

app_page_t *page_power_get(void)
{
    return &s_powerPage;
}
