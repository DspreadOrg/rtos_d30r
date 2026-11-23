/**********************************************************************************************************
 * File Name     : page_home.c
 * Description   : D30R main menu (3 x 3 large cards, 480x854)
 *
 *   Card 132 x 186, horizontal/vertical gap 18, outer margin 24:
 *     24 + 132 + 18 + 132 + 18 + 132 + 24 = 480
 *     24 + 186 + 18 + 186 + 18 + 186 + 24 = 642 (remaining 68 for bottom tip bar)
 **********************************************************************************************************/

#include "LibSysapi.h" /* Lib_MaintenanceMode() */
#include "app_home_icons.h"
#include "app_pages.h"
#include "app_ui.h"

#include <stdint.h>

#define HOME_ITEM_COUNT 9
#define HOME_ITEM_MORE  8 /* 9th item: More -> Maintenance Mode (program download) */
#define HOME_ICON_SIZE  60
#define HOME_BADGE_SIZE 20

typedef struct
{
    const char *title;
    const char *sub;
    const lv_img_dsc_t *icon; /* Hard-coded 1bpp bitmap, no longer read from LittleFS */
    uint32_t iconColor;       /* Recolor color for monochrome bitmap */
} home_item_t;

/* Font library is the LittleFS TTF injected by ResourceInit() (EN/PT/FA only, no Chinese glyphs),
   UI text is all English; font sizes use only the standard levels defined by the SDK */
static const home_item_t s_items[HOME_ITEM_COUNT] = {
    { "Payment", "Card Payment", &home_icon_payment, 0x7C5CFC },
    { "Print", "Receipt / Test", &home_icon_print, 0xF2994A },
    { "Network", "4G / WiFi Setup", &home_icon_network, 0x2F80ED },
    { "TMS", "Remote Update", &home_icon_tms, 0x27AE60 },
    { "Settings", "Display / Time / Lang", &home_icon_settings, 0x8A94A6 },
    { "Device", "PN / SN / Ver", &home_icon_device, 0x5B6BE8 },
    { "Test", "MSR / IC / NFC / QR", &home_icon_test, 0xEB5757 },
    { "About", "Version Info", &home_icon_about, 0x18A999 },
    { "More", "Tools", &home_icon_more, 0x2D9CDB },
};

static lv_obj_t *s_cards[HOME_ITEM_COUNT];
static int s_focus = 0;

static void home_apply_focus(void)
{
    int i;

    for (i = 0; i < HOME_ITEM_COUNT; i++)
    {
        if (s_cards[i] == NULL)
        {
            continue;
        }

        if (i == s_focus)
        {
            lv_obj_set_style_border_width(s_cards[i], 3, 0);
            lv_obj_set_style_border_color(s_cards[i], APP_COLOR_ACCENT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_cards[i], 0, 0);
        }
    }
}

/* Maintenance Mode has moved to the tools page (More -> Maintenance Mode); here only opens the tools page */
static void home_activate(int index)
{
    app_page_t *page = NULL;

    if (index < 0 || index >= HOME_ITEM_COUNT)
    {
        return;
    }

    if (index == HOME_ITEM_MORE)
    {
        page = page_tools_get();
        if (page != NULL)
        {
            app_page_push(page);
        }
        return;
    }

    /* Wired in M2: Network / Settings / Device Info / About; wired in M3: Payment / Print / TMS */
    switch (index)
    {
    case 0:
        page = page_payment_get();
        break;

    case 1:
        page = page_print_get();
        break;

    case 2:
        page = page_network_get();
        break;

    case 3:
        page = page_tms_get();
        break;

    case 4:
        page = page_settings_get();
        break;

    case 5:
        page = page_devinfo_get();
        break;

    case 6:
        page = page_test_get();
        break;

    case 7:
        page = page_about_get();
        break;

    default:
        page = page_placeholder_open(s_items[index].title, s_items[index].sub);
        break;
    }

    if (page != NULL)
    {
        app_page_push(page);
    }
}

static void home_card_clicked_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    home_activate(index);
}

/* Monochrome bitmap with recolor, shown 1:1 */
static lv_obj_t *home_icon_image(lv_obj_t *parent, const lv_img_dsc_t *src, uint32_t color)
{
    lv_obj_t *img = lv_img_create(parent);

    if (img == NULL)
    {
        return NULL;
    }

    lv_img_set_src(img, src);
    lv_obj_set_style_img_recolor(img, lv_color_hex(color), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_center(img);

    return img;
}

static lv_obj_t *home_build_card(lv_obj_t *parent, const home_item_t *item, int index)
{
    lv_obj_t *card       = NULL;
    lv_obj_t *iconCircle = NULL;
    lv_obj_t *label      = NULL;
    lv_obj_t *badge      = NULL;
    lv_coord_t x         = APP_MENU_MARGIN_X + (index % APP_MENU_COLS) * (APP_MENU_CARD_W + APP_MENU_GAP_X);
    lv_coord_t y         = APP_MENU_MARGIN_Y + (index / APP_MENU_COLS) * (APP_MENU_CARD_H + APP_MENU_GAP_Y);
    char keyText[2];

    card = app_card(parent, x, y, APP_MENU_CARD_W, APP_MENU_CARD_H);
    if (card == NULL)
    {
        return NULL;
    }

    /* Badge in top-left corner (corresponds to digit keys 1-9) */
    badge = lv_obj_create(card);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, HOME_BADGE_SIZE, HOME_BADGE_SIZE);
    lv_obj_set_style_radius(badge, HOME_BADGE_SIZE / 2, 0);
    lv_obj_set_style_bg_color(badge, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    label      = lv_label_create(badge);
    keyText[0] = (char)('1' + index);
    keyText[1] = '\0';
    lv_label_set_text(label, keyText);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(14), 0);
    lv_obj_center(label);

    /* Icon */
    iconCircle = app_icon_circle(card, HOME_ICON_SIZE);
    if (iconCircle != NULL)
    {
        lv_obj_align(iconCircle, LV_ALIGN_TOP_MID, 0, 24);
        home_icon_image(iconCircle, item->icon, item->iconColor);
    }

    /* Title */
    label = lv_label_create(card);
    lv_label_set_text(label, LIB_TR(item->title));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(FONT_SIZE_SMALL), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 102);

    /* Description */
    label = lv_label_create(card);
    lv_label_set_text(label, LIB_TR(item->sub));
    lv_obj_set_style_text_color(label, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(label, app_font(16), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 136);

    lv_obj_add_event_cb(card, home_card_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return card;
}

static void home_create(app_page_t *page)
{
    lv_obj_t *tip = NULL;
    int i;

    /* Page scrollable: enable vertical scroll in content area (edge elastic bounce), hide scrollbar */
    lv_obj_add_flag(page->body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page->body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page->body, LV_SCROLLBAR_MODE_OFF);

    for (i = 0; i < HOME_ITEM_COUNT; i++)
    {
        s_cards[i] = home_build_card(page->body, &s_items[i], i);
    }

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Press 1-9 or tap a card to enter"));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(16), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_focus = 0;
    home_apply_focus();
}

static void home_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < HOME_ITEM_COUNT; i++)
    {
        s_cards[i] = NULL;
    }
}

static void home_move_focus(int delta)
{
    int next = s_focus + delta;

    if (next < 0)
    {
        next += HOME_ITEM_COUNT;
    }
    else if (next >= HOME_ITEM_COUNT)
    {
        next -= HOME_ITEM_COUNT;
    }

    s_focus = next;
    home_apply_focus();
}

static bool home_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    /* Digit key direct access */
    if (ev->param >= KB_DIGIT_ONE && ev->param <= KB_DIGIT_NINE)
    {
        home_activate((int)(ev->param - KB_DIGIT_ONE));
        return true;
    }

    switch (ev->param)
    {
    case KB_UP:
        home_move_focus(-APP_MENU_COLS);
        return true;

    case KB_DOWN:
        home_move_focus(APP_MENU_COLS);
        return true;

    case KB_ENTER:
        home_activate(s_focus);
        return true;

    default:
        break;
    }

    return false;
}

static app_page_t s_homePage = {
    .id         = "home",
    .title      = "MAIN MENU",
    .on_create  = home_create,
    .on_event   = home_event,
    .on_destroy = home_destroy,
    .user_data  = NULL,
};

app_page_t *page_home_get(void)
{
    return &s_homePage;
}
