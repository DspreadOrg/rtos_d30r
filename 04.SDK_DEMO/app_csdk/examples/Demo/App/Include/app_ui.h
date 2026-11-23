/**********************************************************************************************************
 * File Name     : app_ui.h
 * Description   : D30R (480x854) theme, layout constants and common widget factory
 **********************************************************************************************************/

#ifndef __APP_UI_H__
#define __APP_UI_H__

#include "lvgl.h"
#include "LibSysapi.h"
#include "LibLanguage.h"    /* LIB_TR() */

/* ---------------- D30R screen and sections (unit: px) ---------------- */
#define APP_SCREEN_W            480
#define APP_SCREEN_H            854

#define APP_STATUS_BAR_H        30      /* Top status bar (drawn by ui_status_bar) */
#define APP_HEADER_H            60      /* Page title bar */
#define APP_NAV_BAR_H           54      /* Bottom navigation bar (drawn by ui_nav_bar) */
#define APP_CONTENT_H           (APP_SCREEN_H - APP_STATUS_BAR_H - APP_NAV_BAR_H - APP_HEADER_H)  /* 710 */

/* ---------------- Main menu grid ---------------- */
#define APP_MENU_COLS           3
#define APP_MENU_ROWS           3
#define APP_MENU_CARD_W         132
#define APP_MENU_CARD_H         186
#define APP_MENU_GAP_X          18
#define APP_MENU_GAP_Y          18
#define APP_MENU_MARGIN_X       24
#define APP_MENU_MARGIN_Y       24

/* ---------------- Theme colors (same as ui_mockups/index.html) ---------------- */
#define APP_COLOR_PRIMARY       lv_color_hex(0x0B3D66)  /* Title bar / nav bar background */
#define APP_COLOR_ACCENT        lv_color_hex(0x1565C0)  /* Button / selected */
#define APP_COLOR_ACCENT_2      lv_color_hex(0x1976D2)  /* Icon / link */
#define APP_COLOR_BG            lv_color_hex(0xF4F7FB)  /* Page background */
#define APP_COLOR_CARD          lv_color_hex(0xFFFFFF)  /* Card */
#define APP_COLOR_TEXT          lv_color_hex(0x1F2D3D)  /* Primary text */
#define APP_COLOR_SUB           lv_color_hex(0x7A8CA3)  /* Secondary text */
#define APP_COLOR_ICON_BG       lv_color_hex(0xEAF3FE)  /* Icon circle background */
#define APP_COLOR_SUCCESS       lv_color_hex(0x26D07C)
#define APP_COLOR_WARN          lv_color_hex(0xE65100)

/* Get the vector font matching the current language, fall back to the default font on failure */
lv_font_t *app_font(unsigned short size);

/* Rounded card (white background + light shadow + clickable) */
lv_obj_t *app_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);

/* Icon circle background (diameter size) */
lv_obj_t *app_icon_circle(lv_obj_t *parent, lv_coord_t size);

/* ---------------- Info row (shared by settings / device info / about / network) ---------------- */
#define APP_ROW_X               16
#define APP_ROW_W               448
#define APP_ROW_H               68
#define APP_ROW_GAP             8
#define APP_ROW_START_Y         16

/* White rounded row: name on the left, value on the right. Returns the right value label for callers to refresh. */
lv_obj_t *app_row(lv_obj_t *parent, lv_coord_t y, const char *name);

/* ---------------- Android-style WiFi signal icon (fan shape, 4 bars) ---------------- */
/* Create a size x size fan signal icon, level 0~4 (0 means no signal), returns the icon container */
lv_obj_t *app_wifi_signal_icon(lv_obj_t *parent, lv_coord_t size, lv_color_t color);

/* Update the number of lit bars of the fan signal icon (0~4) */
void app_wifi_signal_set_level(lv_obj_t *icon, int level);

/* Update the color of the fan signal icon */
void app_wifi_signal_set_color(lv_obj_t *icon, lv_color_t color);

/* Convert the raw signal value from the SDK to 0~4 bars:
   negative as dBm, 0~4 treated as an existing level, 5~31 as CSQ, the rest as a 0~100 percentage */
unsigned char app_signal_level_from_raw(int raw);

/* Global style initialization (reserved, currently an empty implementation) */
void app_ui_init(void);

#endif /* __APP_UI_H__ */
