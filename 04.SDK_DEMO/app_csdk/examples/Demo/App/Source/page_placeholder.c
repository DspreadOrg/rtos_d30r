/**********************************************************************************************************
 * File Name     : page_placeholder.c
 * Description   : Generic placeholder page (holds menu entries before migration and verifies the page stack)
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"

#include <string.h>

#define PLACEHOLDER_POOL        6
#define PLACEHOLDER_ICON_SIZE   96

typedef struct
{
    app_page_t page;
    bool used;
} placeholder_slot_t;

static placeholder_slot_t s_slots[PLACEHOLDER_POOL];

static void placeholder_create(app_page_t *page)
{
    lv_obj_t *iconCircle = NULL;
    lv_obj_t *label = NULL;

    /* LV_SYMBOL_* glyphs exist only in the built-in Montserrat; the business TTF has no PUA glyphs, so no symbol is drawn here */
    iconCircle = app_icon_circle(page->body, PLACEHOLDER_ICON_SIZE);
    if (iconCircle != NULL)
    {
        lv_obj_align(iconCircle, LV_ALIGN_TOP_MID, 0, 140);
    }

    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Under Construction"));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(28), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 270);

    label = lv_label_create(page->body);
    lv_label_set_text(label, (page->user_data != NULL) ? LIB_TR((const char *)page->user_data) : "");
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 316);

    label = lv_label_create(page->body);
    lv_label_set_text(label, LIB_TR("Press BACK or tap the bottom bar to return"));
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(16), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void placeholder_destroy(app_page_t *page)
{
    int i;

    /* Only mark the slot as free; do not clear page itself: app_page_pop() still uses
       page->root to call lv_obj_del() after this callback, so clearing it would make the page undeletable */
    for (i = 0; i < PLACEHOLDER_POOL; i++)
    {
        if (&s_slots[i].page == page)
        {
            s_slots[i].used = false;
            break;
        }
    }
}

app_page_t *page_placeholder_open(const char *title, const char *message)
{
    int i;

    for (i = 0; i < PLACEHOLDER_POOL; i++)
    {
        placeholder_slot_t *slot = &s_slots[i];

        if (slot->used)
        {
            continue;
        }

        memset(slot, 0, sizeof(placeholder_slot_t));
        slot->used = true;

        slot->page.id         = "placeholder";
        slot->page.title      = title;
        slot->page.on_create  = placeholder_create;
        slot->page.on_destroy = placeholder_destroy;
        slot->page.user_data  = (void *)message;

        return &slot->page;
    }

    return NULL;
}
