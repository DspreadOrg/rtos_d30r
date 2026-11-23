/**********************************************************************************************************
 * File Name     : page_language.c
 * Description   : Language selection page, 480x854
 *
 *   The language list comes from the LibLanguage runtime registry (Lib_LangGetCount / Lib_LangGetAt).
 *   Tapping an entry calls Lib_LangSet() to switch the runtime language immediately and writes E_LANG_TYPE for persistence.
 *   The page stack is then rebuilt asynchronously so already rendered pages such as the home page show the new language immediately.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "app_keyboard.h"
#include "LibParamManage.h"
#include "LibLanguage.h"
#include "ui_status_bar.h"

#include <stdint.h>

#define LANG_MAX                LIB_LANG_MAX_COUNT
#define LANG_ROW_H              72
#define LANG_ROW_GAP            6

static lv_obj_t *s_rows[LANG_MAX];
static lv_obj_t *s_marks[LANG_MAX];
static lv_obj_t *s_tip = NULL;
static LIB_LANG_T s_handles[LANG_MAX];
static unsigned short s_count = 0;
static unsigned short s_current = 0;

static void lang_refresh(void)
{
    unsigned short i;

    for (i = 0; i < s_count; i++)
    {
        if (s_rows[i] == NULL)
        {
            continue;
        }

        if (i == s_current)
        {
            lv_obj_set_style_border_width(s_rows[i], 2, 0);
            lv_obj_set_style_border_color(s_rows[i], APP_COLOR_ACCENT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_rows[i], 0, 0);
        }

        if (s_marks[i] != NULL)
        {
            lv_label_set_text(s_marks[i], (i == s_current) ? LIB_TR("Selected") : "");
            lv_obj_set_style_text_color(s_marks[i], APP_COLOR_ACCENT, 0);
        }
    }
}

static void lang_rebuild_cb(void *arg)
{
    (void)arg;

    /* The status bar label was bound to the old font at creation and must be rebound for the new language */
    app_status_bar_refresh_font();
    app_page_rebuild();
}

static void lang_select(int index)
{
    unsigned char value = 0;

    if (index < 0 || index >= (int)s_count)
    {
        return;
    }

    if (!Lib_LangSet(s_handles[index]))
    {
        if (s_tip != NULL)
        {
            lv_label_set_text(s_tip, LIB_TR("Failed to switch language"));
        }
        return;
    }

    /* Save to app parameters synchronously to stay compatible with legacy parameter files */
    value = (unsigned char)s_handles[index];
    Lib_ParamSetU8(E_PARAM_LANG_TYPE, value);
    Lib_ParamSave();

    s_current = (unsigned short)index;

    /* Page texts are already rendered at creation, so rebuild the whole stack; do it asynchronously
       to avoid deleting the current page itself inside a widget callback */
    lv_async_call(lang_rebuild_cb, NULL);
}

static void lang_row_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    lang_select(index);
}

static void lang_create(app_page_t *page)
{
    LIB_LANG_T current = Lib_LangGetCurrent();
    unsigned short i;

    s_count = Lib_LangGetCount();
    if (s_count > LANG_MAX)
    {
        s_count = LANG_MAX;
    }

    s_current = 0;
    for (i = 0; i < s_count; i++)
    {
        s_handles[i] = Lib_LangGetAt(i);

        if (s_handles[i] == current)
        {
            s_current = i;
        }
    }

    for (i = 0; i < s_count; i++)
    {
        const char *name = Lib_LangGetName(s_handles[i]);
        lv_obj_t *row = lv_obj_create(page->body);
        lv_obj_t *label = NULL;

        if (row == NULL)
        {
            s_rows[i] = NULL;
            s_marks[i] = NULL;
            continue;
        }

        lv_obj_remove_style_all(row);
        lv_obj_set_pos(row, APP_ROW_X,
                       APP_ROW_START_Y + (lv_coord_t)i * (LANG_ROW_H + LANG_ROW_GAP));
        lv_obj_set_size(row, APP_ROW_W, LANG_ROW_H);
        lv_obj_set_style_bg_color(row, APP_COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        label = lv_label_create(row);
        lv_label_set_text(label, (name != NULL && name[0] != '\0') ? name : "?");
        lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(label, app_font(20), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);

        s_marks[i] = lv_label_create(row);
        lv_label_set_text(s_marks[i], "");
        lv_obj_set_style_text_color(s_marks[i], APP_COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(s_marks[i], app_font(16), 0);
        lv_obj_align(s_marks[i], LV_ALIGN_RIGHT_MID, -16, 0);

        lv_obj_add_event_cb(row, lang_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        s_rows[i] = row;
    }

    s_tip = lv_label_create(page->body);
    lv_label_set_text(s_tip, LIB_TR("Tap a language to select"));
    lv_obj_set_style_text_color(s_tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(s_tip, app_font(14), 0);
    lv_obj_align(s_tip, LV_ALIGN_BOTTOM_MID, 0, -12);

    lang_refresh();
}

static void lang_destroy(app_page_t *page)
{
    unsigned short i;

    (void)page;

    for (i = 0; i < LANG_MAX; i++)
    {
        s_rows[i] = NULL;
        s_marks[i] = NULL;
    }

    s_tip = NULL;
}

static bool lang_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param >= KB_DIGIT_ONE && (unsigned short)(ev->param - KB_DIGIT_ONE) < s_count)
    {
        lang_select((int)(ev->param - KB_DIGIT_ONE));
        return true;
    }

    return false;
}

static app_page_t s_languagePage =
{
    .id         = "language",
    .title      = "LANGUAGE",
    .on_create  = lang_create,
    .on_event   = lang_event,
    .on_destroy = lang_destroy,
    .user_data  = NULL,
};

app_page_t *page_language_get(void)
{
    return &s_languagePage;
}
