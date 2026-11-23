/**********************************************************************************************************
 * File Name     : page_menu.c
 * Description   : Generic submenu page
 *
 *   page_menu_open() builds a menu page from the given menu data: the title bar uses menu->title,
 *   each item shows title/subtitle; on tap or Enter, item->target() is called to get the target page and push it.
 *   Supports touch tap, UP/DOWN + ENTER, and digit keys 1-8 direct access.
 *
 *   Menu pages use a small object pool instead of a global singleton, allowing nested menus (menu -> submenu) and
 *   the same menu opened multiple times; when the pool is full, page_menu_open() returns NULL.
 *
 *   Data lifetime: menu definitions and text must stay valid while the page is alive (usually a static const array).
 *   The definition itself is copied into the page slot, so page_menu_def_t may be built on the function stack.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"

#include <stdint.h>
#include <string.h>

/* Menus can nest: each level takes one slot; pool size matches page stack depth (APP_PAGE_MAX_DEPTH) */
#define MENU_POOL_SIZE      6
#define MENU_ROW_H          76
#define MENU_ROW_GAP        6
#define MENU_ROW_Y0         16

typedef struct
{
    app_page_t page;                            /* Must be first: callbacks cast directly */
    bool used;
    page_menu_def_t def;
    page_menu_item_t items[PAGE_MENU_MAX_ITEMS];
    lv_obj_t *rows[PAGE_MENU_MAX_ITEMS];
    int focus;
} menu_slot_t;

static menu_slot_t s_slots[MENU_POOL_SIZE];

static void menu_apply_focus(menu_slot_t *slot)
{
    int i;

    for (i = 0; i < slot->def.count; i++)
    {
        if (slot->rows[i] == NULL)
        {
            continue;
        }

        if (i == slot->focus)
        {
            lv_obj_set_style_border_width(slot->rows[i], 3, 0);
            lv_obj_set_style_border_color(slot->rows[i], APP_COLOR_ACCENT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(slot->rows[i], 0, 0);
        }
    }
}

static void menu_activate(menu_slot_t *slot, int index)
{
    app_page_t *target = NULL;

    if (slot == NULL || !slot->used || index < 0 || index >= (int)slot->def.count)
    {
        return;
    }

    if (slot->def.items[index].target == NULL)
    {
        return;
    }

    target = slot->def.items[index].target();
    if (target != NULL)
    {
        app_page_push(target);
    }
}

static void menu_row_cb(lv_event_t *e)
{
    menu_slot_t *slot = (menu_slot_t *)app_page_current();
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    menu_activate(slot, index);
}

static lv_obj_t *menu_build_row(menu_slot_t *slot, int index)
{
    const page_menu_item_t *item = &slot->def.items[index];
    lv_obj_t *row = app_card(slot->page.body, APP_ROW_X,
                             MENU_ROW_Y0 + index * (MENU_ROW_H + MENU_ROW_GAP),
                             APP_ROW_W, MENU_ROW_H);
    lv_obj_t *label = NULL;
    lv_obj_t *arrow = NULL;

    if (row == NULL)
    {
        return NULL;
    }

    label = lv_label_create(row);
    lv_label_set_text(label, (item->title != NULL) ? LIB_TR(item->title) : "");
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);

    if (item->subtitle != NULL)
    {
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 20, -12);

        label = lv_label_create(row);
        lv_label_set_text(label, LIB_TR(item->subtitle));
        lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
        lv_obj_set_style_text_font(label, app_font(14), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 20, 14);
    }
    else
    {
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 20, 0);
    }

    arrow = lv_label_create(row);
    lv_label_set_text(arrow, ">");
    lv_obj_set_style_text_color(arrow, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(arrow, app_font(20), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_add_event_cb(row, menu_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return row;
}

static void menu_create(app_page_t *page)
{
    menu_slot_t *slot = (menu_slot_t *)page;
    lv_obj_t *tip = NULL;
    int i;

    slot->focus = 0;
    memset(slot->rows, 0, sizeof(slot->rows));

    for (i = 0; i < (int)slot->def.count; i++)
    {
        slot->rows[i] = menu_build_row(slot, i);
    }

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Tap an item, or use UP / DOWN + ENTER"));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(14), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -12);

    menu_apply_focus(slot);
}

static void menu_destroy(app_page_t *page)
{
    menu_slot_t *slot = (menu_slot_t *)page;

    /* Only clear slot state, must not memset page: app_page_pop() still uses page->root after the callback */
    slot->used  = false;
    slot->focus = 0;
    memset(slot->rows, 0, sizeof(slot->rows));
    memset(&slot->def, 0, sizeof(slot->def));
    memset(slot->items, 0, sizeof(slot->items));
}

static bool menu_event(app_page_t *page, const app_event_t *ev)
{
    menu_slot_t *slot = (menu_slot_t *)page;
    int count = (int)slot->def.count;

    if (ev->type != APP_EV_KEY || count <= 0)
    {
        return false;
    }

    /* Digit key direct access; keys beyond the item count fall back to global handling */
    if (ev->param >= KB_DIGIT_ONE && ev->param <= (KB_DIGIT_ONE + PAGE_MENU_MAX_ITEMS - 1))
    {
        int index = (int)(ev->param - KB_DIGIT_ONE);

        if (index >= count)
        {
            return false;
        }

        menu_activate(slot, index);
        return true;
    }

    switch (ev->param)
    {
    case KB_UP:
        slot->focus = (slot->focus + count - 1) % count;
        menu_apply_focus(slot);
        return true;

    case KB_DOWN:
        slot->focus = (slot->focus + 1) % count;
        menu_apply_focus(slot);
        return true;

    case KB_ENTER:
        menu_activate(slot, slot->focus);
        return true;

    default:
        break;
    }

    return false;
}

app_page_t *page_menu_open(const page_menu_def_t *menu)
{
    int i;

    if (menu == NULL || menu->items == NULL || menu->count == 0 ||
        menu->count > PAGE_MENU_MAX_ITEMS)
    {
        return NULL;
    }

    for (i = 0; i < MENU_POOL_SIZE; i++)
    {
        menu_slot_t *slot = &s_slots[i];

        if (slot->used)
        {
            continue;
        }

        memset(slot, 0, sizeof(menu_slot_t));
        slot->used = true;

        /* Copy definition and items so no dependency on caller stack frame; strings must still stay valid by caller */
        slot->def = *menu;
        memcpy(slot->items, menu->items, menu->count * sizeof(page_menu_item_t));
        slot->def.items = slot->items;

        slot->page.id         = "menu";
        slot->page.title      = slot->def.title;
        slot->page.on_create  = menu_create;
        slot->page.on_event   = menu_event;
        slot->page.on_destroy = menu_destroy;

        return &slot->page;
    }

    return NULL;
}
