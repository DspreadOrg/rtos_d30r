/**********************************************************************************************************
 * File Name     : page_tools.c
 * Description   : Tools page (entered from the main menu More): maintenance mode / memory monitor
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "app_mem_monitor.h"
#include "LibSysapi.h"     /* Lib_MaintenanceMode() */

#define TOOLS_ITEM_COUNT        2
#define TOOLS_BTN_Y0            90
#define TOOLS_BTN_H             96
#define TOOLS_BTN_GAP           28

enum
{
    TOOLS_ITEM_MAINTENANCE = 0,
    TOOLS_ITEM_MEMMON,
};

static const char *s_titles[TOOLS_ITEM_COUNT] =
{
    "Maintenance Mode", "Memory Monitor",
};

static const char *s_subtitles[TOOLS_ITEM_COUNT] =
{
    "Firmware download", "Toggle RAM overlay",
};

static lv_obj_t *s_items[TOOLS_ITEM_COUNT];
static int s_focus = 0;

static void tools_apply_focus(void)
{
    int i;

    for (i = 0; i < TOOLS_ITEM_COUNT; i++)
    {
        if (s_items[i] == NULL)
        {
            continue;
        }

        if (i == s_focus)
        {
            lv_obj_set_style_border_width(s_items[i], 3, 0);
            lv_obj_set_style_border_color(s_items[i], APP_COLOR_ACCENT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_items[i], 0, 0);
        }
    }
}

static void tools_activate(int index)
{
    app_page_t *page = NULL;

    switch (index)
    {
    case TOOLS_ITEM_MAINTENANCE:
        page = page_maintenance_get();
        if (page != NULL)
        {
            app_page_push(page);
        }
        break;

    case TOOLS_ITEM_MEMMON:
        /* The overlay is attached to lv_layer_top; the overlay manages its own on/off state */
        app_mem_monitor_toggle();
        break;

    default:
        break;
    }
}

static void tools_item_clicked_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    tools_activate(index);
}

static lv_obj_t *tools_build_item(lv_obj_t *parent, int index)
{
    lv_obj_t *card = NULL;
    lv_obj_t *title = NULL;
    lv_obj_t *sub = NULL;

    card = app_card(parent, APP_ROW_X,
                    TOOLS_BTN_Y0 + index * (TOOLS_BTN_H + TOOLS_BTN_GAP),
                    APP_ROW_W, TOOLS_BTN_H);
    if (card == NULL)
    {
        return NULL;
    }

    title = lv_label_create(card);
    lv_label_set_text(title, LIB_TR(s_titles[index]));
    lv_obj_set_style_text_color(title, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, app_font(FONT_SIZE_SMALL), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, -14);

    sub = lv_label_create(card);
    lv_label_set_text(sub, LIB_TR(s_subtitles[index]));
    lv_obj_set_style_text_color(sub, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(sub, app_font(16), 0);
    lv_obj_align(sub, LV_ALIGN_LEFT_MID, 20, 20);

    lv_obj_add_event_cb(card, tools_item_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return card;
}

static void tools_create(app_page_t *page)
{
    lv_obj_t *tip = NULL;
    int i;

    for (i = 0; i < TOOLS_ITEM_COUNT; i++)
    {
        s_items[i] = tools_build_item(page->body, i);
    }

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Tap an item, or use UP / DOWN + ENTER"));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(14), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -16);

    s_focus = 0;
    tools_apply_focus();
}

static void tools_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < TOOLS_ITEM_COUNT; i++)
    {
        s_items[i] = NULL;
    }
}

static bool tools_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    switch (ev->param)
    {
    case KB_UP:
        s_focus = (s_focus + TOOLS_ITEM_COUNT - 1) % TOOLS_ITEM_COUNT;
        tools_apply_focus();
        return true;

    case KB_DOWN:
        s_focus = (s_focus + 1) % TOOLS_ITEM_COUNT;
        tools_apply_focus();
        return true;

    case KB_ENTER:
        tools_activate(s_focus);
        return true;

    case KB_DIGIT_ONE:
        tools_activate(TOOLS_ITEM_MAINTENANCE);
        return true;

    case KB_DIGIT_TWO:
        tools_activate(TOOLS_ITEM_MEMMON);
        return true;

    default:
        break;
    }

    return false;
}

static app_page_t s_toolsPage =
{
    .id         = "tools",
    .title      = "TOOLS",
    .on_create  = tools_create,
    .on_event   = tools_event,
    .on_destroy = tools_destroy,
    .user_data  = NULL,
};

app_page_t *page_tools_get(void)
{
    return &s_toolsPage;
}
