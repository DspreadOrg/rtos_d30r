/**********************************************************************************************************
 * File Name     : page_datetime.c
 * Description   : Date/time setting page, 480x854
 *
 *   6 fields on separate rows, touch +/- to adjust, changes written to RTC immediately (Lib_SetDateTime).
 *   Uses BCD format YYMMDDhhmmss as agreed in LibSysapi.h.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibSysapi.h"

#include <stdint.h>
#include <stdio.h>

#define DT_FIELD_COUNT          6
#define DT_ROW_H                80
#define DT_ROW_GAP              10
#define DT_BTN_W                52
#define DT_BTN_H                52
#define DT_VALUE_W              88
#define DT_YEAR_MIN             2000
#define DT_YEAR_MAX             2099

typedef enum
{
    DT_FIELD_YEAR = 0,
    DT_FIELD_MONTH,
    DT_FIELD_DAY,
    DT_FIELD_HOUR,
    DT_FIELD_MINUTE,
    DT_FIELD_SECOND,
} dt_field_t;

static const char *s_names[DT_FIELD_COUNT] =
{
    "Year", "Month", "Day", "Hour", "Minute", "Second",
};

static lv_obj_t *s_rows[DT_FIELD_COUNT];
static lv_obj_t *s_values[DT_FIELD_COUNT];
static int s_year = DT_YEAR_MIN;
static int s_month = 1;
static int s_day = 1;
static int s_hour = 0;
static int s_minute = 0;
static int s_second = 0;
static int s_focus = 0;

static int *dt_field_ptr(int index)
{
    switch (index)
    {
    case DT_FIELD_YEAR:
        return &s_year;

    case DT_FIELD_MONTH:
        return &s_month;

    case DT_FIELD_DAY:
        return &s_day;

    case DT_FIELD_HOUR:
        return &s_hour;

    case DT_FIELD_MINUTE:
        return &s_minute;

    case DT_FIELD_SECOND:
        return &s_second;

    default:
        return NULL;
    }
}

static int dt_days_in_month(int year, int month)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
    {
        return 29;
    }

    return days[month - 1];
}

static void dt_clamp(void)
{
    if (s_month < 1)
    {
        s_month = 1;
    }
    else if (s_month > 12)
    {
        s_month = 12;
    }

    if (s_day > dt_days_in_month(s_year, s_month))
    {
        s_day = dt_days_in_month(s_year, s_month);
    }

    if (s_day < 1)
    {
        s_day = 1;
    }
}

static unsigned char dt_to_bcd(int value)
{
    return (unsigned char)(((value / 10) << 4) | (value % 10));
}

static void dt_apply(void)
{
    unsigned char bcd[6];

    dt_clamp();

    bcd[0] = dt_to_bcd(s_year % 100);
    bcd[1] = dt_to_bcd(s_month);
    bcd[2] = dt_to_bcd(s_day);
    bcd[3] = dt_to_bcd(s_hour);
    bcd[4] = dt_to_bcd(s_minute);
    bcd[5] = dt_to_bcd(s_second);

    Lib_SetDateTime(bcd);
}

static void dt_show_field(int index)
{
    char text[8] = { 0 };

    if (s_values[index] == NULL)
    {
        return;
    }

    if (index == DT_FIELD_YEAR)
    {
        snprintf(text, sizeof(text), "%04d", s_year);
    }
    else
    {
        snprintf(text, sizeof(text), "%02d", *(dt_field_ptr(index)));
    }

    lv_label_set_text(s_values[index], text);
}

static void dt_show_all(void)
{
    int i;

    for (i = 0; i < DT_FIELD_COUNT; i++)
    {
        dt_show_field(i);
    }
}

static void dt_apply_focus(void)
{
    int i;

    for (i = 0; i < DT_FIELD_COUNT; i++)
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

static void dt_step(int index, int delta)
{
    int *value = dt_field_ptr(index);
    int min = 0;
    int max = 59;

    if (value == NULL)
    {
        return;
    }

    switch (index)
    {
    case DT_FIELD_YEAR:
        min = DT_YEAR_MIN;
        max = DT_YEAR_MAX;
        break;

    case DT_FIELD_MONTH:
        min = 1;
        max = 12;
        break;

    case DT_FIELD_DAY:
        min = 1;
        max = dt_days_in_month(s_year, s_month);
        break;

    case DT_FIELD_HOUR:
        max = 23;
        break;

    default:
        break;
    }

    *value += delta;
    if (*value > max)
    {
        *value = min;
    }
    else if (*value < min)
    {
        *value = max;
    }

    dt_apply();
    dt_show_all();
}

static void dt_inc_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    s_focus = index;
    dt_apply_focus();
    dt_step(index, 1);
}

static void dt_dec_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    s_focus = index;
    dt_apply_focus();
    dt_step(index, -1);
}

static lv_obj_t *dt_button(lv_obj_t *row, const char *text, lv_coord_t xOfs)
{
    lv_obj_t *btn = lv_obj_create(row);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, DT_BTN_W, DT_BTN_H);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, xOfs, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_center(label);

    return btn;
}

static lv_obj_t *dt_row(lv_obj_t *parent, lv_coord_t y, int index)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *name = NULL;
    lv_obj_t *btn = NULL;

    if (row == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, APP_ROW_X, y);
    lv_obj_set_size(row, APP_ROW_W, DT_ROW_H);
    lv_obj_set_style_bg_color(row, APP_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    name = lv_label_create(row);
    lv_label_set_text(name, s_names[index]);
    lv_obj_set_style_text_color(name, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(name, app_font(20), 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 16, 0);

    s_values[index] = lv_label_create(row);
    lv_obj_set_width(s_values[index], DT_VALUE_W);
    lv_obj_set_style_text_align(s_values[index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_values[index], APP_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_values[index], app_font(22), 0);
    lv_obj_align(s_values[index], LV_ALIGN_RIGHT_MID, -152, 0);

    btn = dt_button(row, "+", -16);
    if (btn != NULL)
    {
        lv_obj_add_event_cb(btn, dt_inc_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);
    }

    btn = dt_button(row, "-", -80);
    if (btn != NULL)
    {
        lv_obj_add_event_cb(btn, dt_dec_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);
    }

    return row;
}

static void dt_load(void)
{
    unsigned char bcd[8] = { 0 };

    if (Lib_GetDateTime(bcd) != 0)
    {
        return;
    }

    /* Decode BCD nibbles to decimal */
    s_year = DT_YEAR_MIN + (((int)bcd[0] >> 4) * 10 + (bcd[0] & 0x0F));
    s_month = ((int)bcd[1] >> 4) * 10 + (bcd[1] & 0x0F);
    s_day = ((int)bcd[2] >> 4) * 10 + (bcd[2] & 0x0F);
    s_hour = ((int)bcd[3] >> 4) * 10 + (bcd[3] & 0x0F);
    s_minute = ((int)bcd[4] >> 4) * 10 + (bcd[4] & 0x0F);
    s_second = ((int)bcd[5] >> 4) * 10 + (bcd[5] & 0x0F);

    if (s_year < DT_YEAR_MIN || s_year > DT_YEAR_MAX ||
        s_month < 1 || s_month > 12 ||
        s_day < 1 || s_day > 31 ||
        s_hour > 23 || s_minute > 59 || s_second > 59)
    {
        s_year = DT_YEAR_MIN;
        s_month = 1;
        s_day = 1;
        s_hour = 0;
        s_minute = 0;
        s_second = 0;
    }
}

static void dt_create(app_page_t *page)
{
    int i;

    dt_load();

    for (i = 0; i < DT_FIELD_COUNT; i++)
    {
        s_rows[i] = dt_row(page->body,
                           APP_ROW_START_Y + (lv_coord_t)i * (DT_ROW_H + DT_ROW_GAP),
                           i);
    }

    dt_show_all();

    s_focus = DT_FIELD_YEAR;
    dt_apply_focus();
}

static void dt_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < DT_FIELD_COUNT; i++)
    {
        s_rows[i] = NULL;
        s_values[i] = NULL;
    }
}

static bool dt_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    switch (ev->param)
    {
    case KB_UP:
        s_focus = (s_focus + DT_FIELD_COUNT - 1) % DT_FIELD_COUNT;
        dt_apply_focus();
        return true;

    case KB_DOWN:
        s_focus = (s_focus + 1) % DT_FIELD_COUNT;
        dt_apply_focus();
        return true;

    case KB_ENTER:
        dt_step(s_focus, 1);
        return true;

    default:
        break;
    }

    return false;
}

static app_page_t s_datetimePage =
{
    .id         = "datetime",
    .title      = "DATE & TIME",
    .on_create  = dt_create,
    .on_event   = dt_event,
    .on_destroy = dt_destroy,
    .user_data  = NULL,
};

app_page_t *page_datetime_get(void)
{
    return &s_datetimePage;
}
