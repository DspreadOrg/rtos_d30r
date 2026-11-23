/**********************************************************************************************************
 * File Name     : app_keyboard.c
 * Description   : Generic input keyboard implementation
 **********************************************************************************************************/

#include "app_keyboard.h"
#include "app_ui.h"

#include <string.h>

#define KB_PANEL_W          460
#define KB_PANEL_H          470
#define KB_BOX_W            440
#define KB_BOX_H            290
#define KB_TEXT_MAX         128

static lv_obj_t *s_mask = NULL;
static lv_obj_t *s_textArea = NULL;
static app_kb_done_cb s_doneCb = NULL;
static app_kb_cancel_cb s_cancelCb = NULL;
static void *s_userData = NULL;

/* Keyboard keys uniformly use the built-in Montserrat:
   although the business TTF has empty glyphs for some PUA code points (probe passes),
   FontAwesome icons such as backspace/enter/Shift are missing and render as boxes on real devices; the built-in font carries the full symbol set, ensuring no garbled characters */
static lv_font_t *keyboard_font(void)
{
    return (lv_font_t *)LV_FONT_DEFAULT;
}

bool app_keyboard_is_open(void)
{
    return (s_mask != NULL);
}

void app_keyboard_close(void)
{
    if (s_mask != NULL)
    {
        lv_obj_del(s_mask);
        s_mask = NULL;
    }

    s_textArea = NULL;
    s_doneCb = NULL;
    s_cancelCb = NULL;
    s_userData = NULL;
}

static void kb_finish(bool accepted)
{
    char acText[KB_TEXT_MAX] = { 0 };
    app_kb_done_cb doneCb = s_doneCb;
    app_kb_cancel_cb cancelCb = s_cancelCb;
    void *userData = s_userData;

    if (accepted && s_textArea != NULL)
    {
        const char *text = lv_textarea_get_text(s_textArea);

        if (text != NULL)
        {
            strncpy(acText, text, KB_TEXT_MAX - 1);
        }
    }

    /* Close first, then call back: the callback may immediately open a new keyboard */
    app_keyboard_close();

    if (accepted)
    {
        if (doneCb != NULL)
        {
            doneCb(acText, userData);
        }
    }
    else if (cancelCb != NULL)
    {
        cancelCb(userData);
    }
}

static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY)
    {
        kb_finish(true);
    }
    else if (code == LV_EVENT_CANCEL)
    {
        kb_finish(false);
    }
}

bool app_keyboard_open(const char *title,
                       const char *initial_text,
                       bool password,
                       unsigned max_len,
                       app_kb_done_cb on_done,
                       app_kb_cancel_cb on_cancel,
                       void *user_data)
{
    lv_obj_t *panel = NULL;
    lv_obj_t *label = NULL;
    lv_obj_t *kb = NULL;
    lv_font_t *keyFont = keyboard_font();

    if (s_mask != NULL)
    {
        return false;
    }

    s_doneCb = on_done;
    s_cancelCb = on_cancel;
    s_userData = user_data;

    s_mask = lv_obj_create(lv_layer_top());
    if (s_mask == NULL)
    {
        s_doneCb = NULL;
        s_cancelCb = NULL;
        s_userData = NULL;
        return false;
    }

    lv_obj_remove_style_all(s_mask);
    lv_obj_set_size(s_mask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_mask, 0, 0);
    lv_obj_set_style_bg_color(s_mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_mask, LV_OPA_50, 0);
    lv_obj_add_flag(s_mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_mask, LV_OBJ_FLAG_SCROLLABLE);

    panel = lv_obj_create(s_mask);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, KB_PANEL_W, KB_PANEL_H);
    lv_obj_set_style_bg_color(panel, APP_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(panel);

    label = lv_label_create(panel);
    lv_label_set_text(label, (title != NULL) ? LIB_TR(title) : LIB_TR("Input"));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(18), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    s_textArea = lv_textarea_create(panel);
    lv_obj_set_size(s_textArea, KB_BOX_W, 56);
    lv_obj_align(s_textArea, LV_ALIGN_TOP_MID, 0, 58);
    lv_textarea_set_one_line(s_textArea, true);
    lv_textarea_set_max_length(s_textArea, (max_len > 0) ? (uint32_t)max_len : 32);
    lv_textarea_set_password_mode(s_textArea, password);
    lv_obj_set_style_bg_color(s_textArea, APP_COLOR_BG, 0);
    lv_obj_set_style_border_width(s_textArea, 1, 0);
    lv_obj_set_style_border_color(s_textArea, lv_color_hex(0xD5DDE8), 0);
    lv_obj_set_style_radius(s_textArea, 8, 0);
    lv_obj_set_style_pad_left(s_textArea, 12, 0);
    lv_obj_set_style_text_font(s_textArea, app_font(20), 0);
    lv_obj_set_style_text_color(s_textArea, APP_COLOR_TEXT, 0);

    if (initial_text != NULL && initial_text[0] != '\0')
    {
        lv_textarea_set_text(s_textArea, initial_text);
        lv_textarea_set_cursor_pos(s_textArea, LV_TEXTAREA_CURSOR_LAST);
    }

    kb = lv_keyboard_create(panel);
    lv_obj_set_size(kb, KB_BOX_W, KB_BOX_H);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_keyboard_set_textarea(kb, s_textArea);

    /* Keyboard appearance: white rounded keys + pressed highlight, font size follows available fonts */
    lv_obj_set_style_bg_color(kb, APP_COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_border_width(kb, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(kb, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(kb, 6, LV_PART_MAIN);

    lv_obj_set_style_bg_color(kb, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(0xD5DDE8), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, APP_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, keyFont, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(kb, APP_COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, lv_color_white(), LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_obj_set_style_bg_color(kb, APP_COLOR_ICON_BG, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, APP_COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, NULL);

    return true;
}
