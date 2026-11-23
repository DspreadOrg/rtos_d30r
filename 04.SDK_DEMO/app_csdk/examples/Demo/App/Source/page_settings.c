/**********************************************************************************************************
 * File Name     : page_settings.c
 * Description   : Settings page (brightness / buzzer / sleep time / network / date & time / language), 480x854
 *
 *   Brightness: applied to the screen immediately, written back on page exit.
 *   Buzzer: on/off state is written back on page exit.
 *   Sleep time: power-down parameter E_SLEEP_TIME, 0 means no sleep, default 60 s.
 *   The last three items are sub-entries, pushed onto the stack when clicked.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibParamManage.h"
#include "LibSysapi.h"
#include "LibLcd.h"

#include <stdint.h>

#define SET_ITEM_COUNT          6
#define SET_ITEM_BRIGHT         0
#define SET_ITEM_BEEP           1
#define SET_ITEM_SLEEP          2
#define SET_ITEM_NETWORK        3
#define SET_ITEM_DATETIME       4
#define SET_ITEM_LANGUAGE       5
#define SET_ROW_H               84
#define SET_FIRST_Y             APP_ROW_START_Y
#define SET_ROW_GAP             10
#define SET_BRIGHT_MIN          1
#define SET_BRIGHT_MAX          7       // D30R has only 7 levels, other products have 8
#define SET_BTN_H               52
#define SET_BTN_W               52
#define SET_BEEP_BTN_W          96
#define SET_SLEEP_STEP          15      /* sleep time adjustment step (seconds) */

// setting Sleep time (in seconds)
// 0 indicates no sleep
#define SDK_TIME_SEC_SLEEP_MIN          0
#define SDK_TIME_SEC_SLEEP_MAX          250
#define SDK_TIME_SEC_SLEEP_DEF          60
#define TIME_SEC_SLEEP_DEF              60

static lv_obj_t *s_rows[SET_ITEM_COUNT];
static lv_obj_t *s_brightValue = NULL;
static lv_obj_t *s_beepButton = NULL;
static lv_obj_t *s_sleepValue = NULL;
static int s_focus = 0;
static unsigned char s_bright = 5;
static unsigned char s_beep = 1;
static int s_sleep = SDK_TIME_SEC_SLEEP_DEF;
static bool s_dirty = false;

static lv_obj_t *settings_row(lv_obj_t *parent, lv_coord_t y, const char *name)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (row == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, APP_ROW_X, y);
    lv_obj_set_size(row, APP_ROW_W, SET_ROW_H);
    lv_obj_set_style_bg_color(row, APP_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(row);
    lv_label_set_text(label, LIB_TR(name));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);

    return row;
}

static lv_obj_t *settings_button(lv_obj_t *parent, const char *text, lv_coord_t w,
                                 lv_align_t align, lv_coord_t xOfs)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, SET_BTN_H);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_align(btn, align, xOfs, 0);

    label = lv_label_create(btn);
    lv_label_set_text(label, LIB_TR(text));
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_center(label);

    return btn;
}

static void settings_open(int index)
{
    app_page_t *page = NULL;

    switch (index)
    {
    case SET_ITEM_NETWORK:
        page = page_network_get();
        break;

    case SET_ITEM_DATETIME:
        page = page_datetime_get();
        break;

    case SET_ITEM_LANGUAGE:
        page = page_language_get();
        break;

    default:
        return;
    }

    if (page != NULL)
    {
        app_page_push(page);
    }
}

static void settings_nav_cb(lv_event_t *e)
{
    settings_open((int)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *settings_nav_row(lv_obj_t *parent, lv_coord_t y, const char *name, int index)
{
    lv_obj_t *row = settings_row(parent, y, name);
    lv_obj_t *arrow = NULL;

    if (row == NULL)
    {
        return NULL;
    }

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    arrow = lv_label_create(row);
    lv_label_set_text(arrow, ">");
    lv_obj_set_style_text_color(arrow, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(arrow, app_font(20), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_add_event_cb(row, settings_nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return row;
}

static void settings_apply_focus(void)
{
    int i;

    for (i = 0; i < SET_ITEM_COUNT; i++)
    {
        if (s_rows[i] == NULL)
        {
            continue;
        }

        if (i == s_focus)
        {
            lv_obj_set_style_border_width(s_rows[i], 2, 0);
            lv_obj_set_style_border_color(s_rows[i], APP_COLOR_ACCENT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_rows[i], 0, 0);
        }
    }
}

static void settings_show_bright(void)
{
    if (s_brightValue != NULL)
    {
        lv_label_set_text_fmt(s_brightValue, "%d", s_bright);
    }
}

static void settings_set_bright(int level)
{
    if (level < SET_BRIGHT_MIN)
    {
        level = SET_BRIGHT_MIN;
    }
    else if (level > SET_BRIGHT_MAX)
    {
        level = SET_BRIGHT_MAX;
    }

    s_bright = (unsigned char)level;
    s_dirty = true;

    /* Same as the original UISetGrade(E_BRIGHTNESS): update param cache first, then send to screen */
    Lib_ParamSetU8(E_PARAM_BRIGHT_LEVEL, s_bright);
    Lib_LcdSetBrightLevel(s_bright);

    settings_show_bright();
}

static void settings_show_beep(void)
{
    lv_obj_t *label = NULL;

    if (s_beepButton == NULL)
    {
        return;
    }

    label = lv_obj_get_child(s_beepButton, 0);
    if (label != NULL)
    {
        lv_label_set_text(label, (s_beep == 1) ? LIB_TR("ON") : LIB_TR("OFF"));
    }
}

static void settings_toggle_beep(void)
{
    s_beep = (s_beep == 1) ? 0 : 1;
    s_dirty = true;
    settings_show_beep();
}

static void settings_show_sleep(void)
{
    if (s_sleepValue == NULL)
    {
        return;
    }

    /* 0 means no sleep */
    if (s_sleep <= 0)
    {
        lv_label_set_text(s_sleepValue, LIB_TR("OFF"));
    }
    else
    {
        lv_label_set_text_fmt(s_sleepValue, "%ds", s_sleep);
    }
}

static void settings_set_sleep(int seconds)
{
    if (seconds < SDK_TIME_SEC_SLEEP_MIN)
    {
        seconds = SDK_TIME_SEC_SLEEP_MIN;
    }
    else if (seconds > SDK_TIME_SEC_SLEEP_MAX)
    {
        seconds = SDK_TIME_SEC_SLEEP_MAX;
    }

    s_sleep = seconds;
    s_dirty = true;
    settings_show_sleep();
}

static void settings_sleep_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);

    settings_set_sleep(s_sleep + delta * SET_SLEEP_STEP);
}

static void settings_bright_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);

    settings_set_bright((int)s_bright + delta);
}

static void settings_beep_cb(lv_event_t *e)
{
    (void)e;

    settings_toggle_beep();
}

static void settings_create(app_page_t *page)
{
    lv_obj_t *row = NULL;

    /* Read current brightness, fall back to middle level on invalid value */
    s_bright = 5;
    if (Lib_ParamGetU8(E_PARAM_BRIGHT_LEVEL, &s_bright) != RET_OK)
    {
        s_bright = 5;
    }

    s_beep = 1;
    if (Lib_ParamGetU8(E_PARAM_BEEP, &s_beep) != RET_OK)
    {
        s_beep = 0;
    }
    s_beep = (s_beep == 1) ? 1 : 0;

    /* Sleep time: SDK parameter, GetPwrParam returns the 60 s default when not configured */
    Lib_ParamGetU32(E_PARAM_SLEEP_TIME, (unsigned int *)&s_sleep);
    if (s_sleep > SDK_TIME_SEC_SLEEP_MAX)
    {
        s_sleep = SDK_TIME_SEC_SLEEP_DEF;
    }
    s_dirty = false;

    /* Brightness row */
    row = settings_row(page->body, SET_FIRST_Y, "Brightness");
    s_rows[SET_ITEM_BRIGHT] = row;
    if (row != NULL)
    {
        lv_obj_t *btn = NULL;

        s_brightValue = lv_label_create(row);
        lv_obj_set_width(s_brightValue, 56);
        lv_obj_set_style_text_align(s_brightValue, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_brightValue, APP_COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(s_brightValue, app_font(22), 0);
        lv_obj_align(s_brightValue, LV_ALIGN_RIGHT_MID, -136, 0);
        settings_show_bright();

        btn = settings_button(row, "+", SET_BTN_W, LV_ALIGN_RIGHT_MID, -16);
        if (btn != NULL)
        {
            lv_obj_add_event_cb(btn, settings_bright_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
        }

        btn = settings_button(row, "-", SET_BTN_W, LV_ALIGN_RIGHT_MID, -80);
        if (btn != NULL)
        {
            lv_obj_add_event_cb(btn, settings_bright_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
        }
    }

    /* Buzzer row */
    row = settings_row(page->body, SET_FIRST_Y + SET_ROW_H + SET_ROW_GAP, "Buzzer");
    s_rows[SET_ITEM_BEEP] = row;
    if (row != NULL)
    {
        s_beepButton = settings_button(row, "ON", SET_BEEP_BTN_W, LV_ALIGN_RIGHT_MID, -16);
        if (s_beepButton != NULL)
        {
            lv_obj_add_event_cb(s_beepButton, settings_beep_cb, LV_EVENT_CLICKED, NULL);
        }
        settings_show_beep();
    }

    /* Sleep time row */
    row = settings_row(page->body, SET_FIRST_Y + 2 * (SET_ROW_H + SET_ROW_GAP), "Sleep Time");
    s_rows[SET_ITEM_SLEEP] = row;
    if (row != NULL)
    {
        lv_obj_t *btn = NULL;

        s_sleepValue = lv_label_create(row);
        lv_obj_set_width(s_sleepValue, 64);
        lv_obj_set_style_text_align(s_sleepValue, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_sleepValue, APP_COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(s_sleepValue, app_font(22), 0);
        lv_obj_align(s_sleepValue, LV_ALIGN_RIGHT_MID, -136, 0);
        settings_show_sleep();

        btn = settings_button(row, "+", SET_BTN_W, LV_ALIGN_RIGHT_MID, -16);
        if (btn != NULL)
        {
            lv_obj_add_event_cb(btn, settings_sleep_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
        }

        btn = settings_button(row, "-", SET_BTN_W, LV_ALIGN_RIGHT_MID, -80);
        if (btn != NULL)
        {
            lv_obj_add_event_cb(btn, settings_sleep_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
        }
    }

    /* Sub-entries: network / date & time / language */
    s_rows[SET_ITEM_NETWORK] = settings_nav_row(page->body,
                                                SET_FIRST_Y + 3 * (SET_ROW_H + SET_ROW_GAP),
                                                "Network", SET_ITEM_NETWORK);
    s_rows[SET_ITEM_DATETIME] = settings_nav_row(page->body,
                                                 SET_FIRST_Y + 4 * (SET_ROW_H + SET_ROW_GAP),
                                                 "Date & Time", SET_ITEM_DATETIME);
    s_rows[SET_ITEM_LANGUAGE] = settings_nav_row(page->body,
                                                 SET_FIRST_Y + 5 * (SET_ROW_H + SET_ROW_GAP),
                                                 "Language", SET_ITEM_LANGUAGE);

    s_focus = 0;
    settings_apply_focus();
}

static void settings_destroy(app_page_t *page)
{
    unsigned char value = 0;
    int i;

    (void)page;

    /* Write changes back to the param file on page exit, avoiding Flash erase/write on every key press */
    if (s_dirty)
    {
        Lib_ParamSetU8(E_PARAM_BRIGHT_LEVEL, s_bright);
        Lib_ParamSetU8(E_PARAM_BEEP, s_beep);

        /* Sleep time is an SDK parameter, reset the sleep timer with the new value after writing back */
        value = (unsigned char)s_sleep;
        Lib_SetInfo(E_SLEEP_TIME, &value, sizeof(unsigned char));
        Lib_ParamSetU32(E_PARAM_SLEEP_TIME, value);

        Lib_ParamSave();
        s_dirty = false;
    }

    for (i = 0; i < SET_ITEM_COUNT; i++)
    {
        s_rows[i] = NULL;
    }
    s_brightValue = NULL;
    s_beepButton = NULL;
    s_sleepValue = NULL;
}

static bool settings_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    switch (ev->param)
    {
    case KB_UP:
        s_focus = (s_focus + SET_ITEM_COUNT - 1) % SET_ITEM_COUNT;
        settings_apply_focus();
        return true;

    case KB_DOWN:
        s_focus = (s_focus + 1) % SET_ITEM_COUNT;
        settings_apply_focus();
        return true;

    case KB_ENTER:
        if (s_focus == SET_ITEM_BRIGHT)
        {
            /* Wrap to the minimum level at the top */
            settings_set_bright((s_bright >= SET_BRIGHT_MAX) ? SET_BRIGHT_MIN : (int)s_bright + 1);
        }
        else if (s_focus == SET_ITEM_BEEP)
        {
            settings_toggle_beep();
        }
        else if (s_focus == SET_ITEM_SLEEP)
        {
            /* Wrap to 0 (no sleep) at the top */
            settings_set_sleep((s_sleep >= SDK_TIME_SEC_SLEEP_MAX)
                                   ? SDK_TIME_SEC_SLEEP_MIN
                                   : (s_sleep + SET_SLEEP_STEP));
        }
        else
        {
            settings_open(s_focus);
        }
        return true;

    default:
        break;
    }

    /* Number keys set brightness directly */
    if (ev->param >= KB_DIGIT_ONE && ev->param <= KB_DIGIT_NINE)
    {
        int level = (int)(ev->param - KB_DIGIT_ONE) + 1;

        if (level <= SET_BRIGHT_MAX)
        {
            s_focus = 0;
            settings_apply_focus();
            settings_set_bright(level);
            return true;
        }
    }

    return false;
}

static app_page_t s_settingsPage =
{
    .id         = "settings",
    .title      = "SETTINGS",
    .on_create  = settings_create,
    .on_event   = settings_event,
    .on_destroy = settings_destroy,
    .user_data  = NULL,
};

app_page_t *page_settings_get(void)
{
    return &s_settingsPage;
}
