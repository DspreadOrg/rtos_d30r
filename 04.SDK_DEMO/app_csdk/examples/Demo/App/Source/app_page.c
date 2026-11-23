/**********************************************************************************************************
 * File Name     : app_page.c
 * Description   : Page stack implementation
 *
 *   Layout convention (D30R 480x854):
 *     SDK page body (480 x 770, status bar 30 and nav bar 54 already deducted)
 *       ├── header  y=0    height 60    ← created by this module
 *       └── body    y=60   height 710   ← page widgets are attached here
 **********************************************************************************************************/

#include "app_page.h"
#include "app_pages.h"
#include "app_ui.h"
#include "lv_port_indev.h"

#include <string.h>

static app_page_t *s_stack[APP_PAGE_MAX_DEPTH];
static uint8_t s_depth = 0;

void app_page_init(void)
{
    memset(s_stack, 0, sizeof(s_stack));
    s_depth = 0;
}

void app_page_reset(void)
{
    memset(s_stack, 0, sizeof(s_stack));
    s_depth = 0;
}

extern lv_obj_t *LvUiGetPageBody(void);
static lv_obj_t *page_parent(void)
{
    lv_obj_t *body = LvUiGetPageBody();

    return (body != NULL) ? body : lv_scr_act();
}

/* Hardware keys are collected by the keypad indev; here they are converted into app events */
static void page_key_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);

    /* The power key is handled only by app_power.c polling (short press: screen off / long press: show menu).
       The SDK keypad scan also reports the power key as KB_POWER_OFF; if it is posted again here,
       the event stays in the queue on short-press screen off and is dispatched as the power menu only after the next wakeup */
    if (key == KB_POWER_OFF)
    {
        Lib_Trace("[PWR] drop KB_POWER_OFF from keypad\n");
        return;
    }

    if (key != 0)
    {
        app_event_post_key((unsigned char)key);
    }
}

/* Give keyboard focus to the current page root object so that LV_EVENT_KEY can be dispatched to the page */
static void page_focus(app_page_t *page)
{
    if (page != NULL && page->root != NULL)
    {
        LvGuiGroupSetObj(page->root);
    }
}

static void page_build(app_page_t *page)
{
    lv_obj_t *titleLabel = NULL;

    page->root = lv_obj_create(page_parent());
    if (page->root == NULL)
    {
        return;
    }

    lv_obj_remove_style_all(page->root);
    lv_obj_set_pos(page->root, 0, 0);
    lv_obj_set_size(page->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(page->root, APP_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page->root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page->root, page_key_cb, LV_EVENT_KEY, NULL);

    /* Header */
    page->header = lv_obj_create(page->root);
    lv_obj_remove_style_all(page->header);
    lv_obj_set_pos(page->header, 0, 0);
    lv_obj_set_size(page->header, LV_PCT(100), APP_HEADER_H);
    lv_obj_set_style_bg_color(page->header, APP_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(page->header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page->header, LV_OBJ_FLAG_SCROLLABLE);

    titleLabel = lv_label_create(page->header);
    lv_label_set_text(titleLabel, LIB_TR((page->title != NULL) ? page->title : ""));
    lv_obj_set_width(titleLabel, LV_PCT(100));
    lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(titleLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(titleLabel, app_font(FONT_SIZE_SMALL), 0);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, 0);

    /* Content area */
    page->body = lv_obj_create(page->root);
    lv_obj_remove_style_all(page->body);
    lv_obj_set_pos(page->body, 0, APP_HEADER_H);
    lv_obj_set_size(page->body, LV_PCT(100), APP_CONTENT_H);
    lv_obj_set_style_bg_color(page->body, APP_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page->body, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page->body, LV_OBJ_FLAG_SCROLLABLE);

    if (page->on_create != NULL)
    {
        page->on_create(page);
    }
}

bool app_page_push(app_page_t *page)
{
    if (page == NULL || s_depth >= APP_PAGE_MAX_DEPTH)
    {
        return false;
    }

    if (s_depth > 0)
    {
        lv_obj_add_flag(s_stack[s_depth - 1]->root, LV_OBJ_FLAG_HIDDEN);
    }

    page_build(page);

    if (page->root == NULL)
    {
        if (s_depth > 0)
        {
            lv_obj_clear_flag(s_stack[s_depth - 1]->root, LV_OBJ_FLAG_HIDDEN);
        }
        return false;
    }

    s_stack[s_depth] = page;
    s_depth++;

    page_focus(page);

    return true;
}

bool app_page_pop(void)
{
    app_page_t *top = NULL;

    if (s_depth <= 1)
    {
        return false;
    }

    s_depth--;
    top = s_stack[s_depth];
    s_stack[s_depth] = NULL;

    if (top->on_destroy != NULL)
    {
        top->on_destroy(top);
    }

    if (top->root != NULL)
    {
        lv_obj_del(top->root);
        top->root   = NULL;
        top->header = NULL;
        top->body   = NULL;
    }

    if (s_depth > 0)
    {
        lv_obj_clear_flag(s_stack[s_depth - 1]->root, LV_OBJ_FLAG_HIDDEN);
        page_focus(s_stack[s_depth - 1]);
    }

    return true;
}

void app_page_pop_to_root(void)
{
    while (app_page_pop())
    {
    }
}

void app_page_rebuild(void)
{
    /* Destroy one by one, including the root page: the header and all widgets must be recreated in the new language */
    while (s_depth > 0)
    {
        app_page_t *top = NULL;

        s_depth--;
        top = s_stack[s_depth];
        s_stack[s_depth] = NULL;

        if (top->on_destroy != NULL)
        {
            top->on_destroy(top);
        }

        if (top->root != NULL)
        {
            lv_obj_del(top->root);
            top->root   = NULL;
            top->header = NULL;
            top->body   = NULL;
        }
    }

    app_page_push(page_home_get());
}

app_page_t *app_page_current(void)
{
    return (s_depth > 0) ? s_stack[s_depth - 1] : NULL;
}

uint8_t app_page_depth(void)
{
    return s_depth;
}

void app_page_dispatch(const app_event_t *ev)
{
    app_page_t *page = app_page_current();
    bool consumed = false;

    if (page == NULL || ev == NULL)
    {
        return;
    }

    /* Tamper alarm: globally show the alarm page; ignored if already on the alarm page */
    if (ev->type == APP_EV_TAMPER)
    {
        Lib_Trace("[TAMPER] dispatch type=0x%02X page=%s depth=%d\n",
                  ev->param, (page->id != NULL) ? page->id : "?", s_depth);

        if (page->id == NULL || strcmp(page->id, "tamper") != 0)
        {
            app_page_push(page_tamper_get());
        }
        return;
    }

    /* Power key: globally show the power menu; ignored if already on the power page */
    if (ev->type == APP_EV_KEY && ev->param == KB_POWER_OFF)
    {
        Lib_Trace("[PWR] dispatch KB_POWER_OFF page=%s depth=%d\n",
                  (page->id != NULL) ? page->id : "?", s_depth);

        if (page->id == NULL || strcmp(page->id, "power") != 0)
        {
            app_page_push(page_power_get());
        }
        return;
    }

    if (page->on_event != NULL)
    {
        consumed = page->on_event(page, ev);
    }

    if (consumed || ev->type != APP_EV_KEY)
    {
        return;
    }

    /* Keys not handled by the page follow the global convention */
    switch (ev->param)
    {
    case KB_CANCEL:
        app_page_pop();
        break;

    case KB_MENU:
    case KB_CLEAR:
        app_page_pop_to_root();
        break;

    default:
        break;
    }
}
