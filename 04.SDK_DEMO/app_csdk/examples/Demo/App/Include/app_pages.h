/**********************************************************************************************************
 * File Name     : app_pages.h
 * Description   : Page instance factory
 **********************************************************************************************************/

#ifndef __APP_PAGES_H__
#define __APP_PAGES_H__

#include "app_page.h"

/* Main menu (root page of the page stack, always resident) */
app_page_t *page_home_get(void);

/* Generic placeholder page: title + description. Returns NULL if the pool is full. */
app_page_t *page_placeholder_open(const char *title, const char *message);

/* ---------------- Generic submenu page ---------------- */

#define PAGE_MENU_MAX_ITEMS     8

/* Menu item target page factory; returns NULL if the item has no action */
typedef app_page_t *(*page_menu_target_cb)(void);

typedef struct
{
    const char *title;              /* Menu item title */
    const char *subtitle;           /* Optional description; not shown if NULL */
    page_menu_target_cb target;     /* Target page to push; NULL means no action */
} page_menu_item_t;

typedef struct
{
    const char *title;              /* Page title bar text */
    const page_menu_item_t *items;  /* Menu item array */
    uint8_t count;                  /* Menu item count, 1 ~ PAGE_MENU_MAX_ITEMS */
} page_menu_def_t;

/* Create a menu page from the given menu data; returns NULL if parameters are invalid or the pool is full.
 * The menu definition is copied, but the title/subtitle strings must stay valid for the page's lifetime.
 *
 * Usage:
 *   static const page_menu_item_t s_items[] = {
 *       { "WiFi", "AP list", page_wifi_ap_get },
 *       { "Detail", NULL, page_wifi_info_get },
 *   };
 *   static const page_menu_def_t s_menu = { "NETWORK", s_items, 2 };
 *   app_page_t *page = page_menu_open(&s_menu);
 *   if (page != NULL) app_page_push(page);
 */
app_page_t *page_menu_open(const page_menu_def_t *menu);

/* Maintenance (download) mode page */
app_page_t *page_maintenance_get(void);

/* Is maintenance mode running (during runtime, nav keys are used to restart and exit) */
bool page_maintenance_is_active(void);

/* Settings page (brightness / buzzer / sleep duration / network / date-time / language) */
app_page_t *page_settings_get(void);

/* Date-time settings page */
app_page_t *page_datetime_get(void);

/* Language selection page */
app_page_t *page_language_get(void);

/* Device info page (PN / SN / IMEI / battery / capacity) */
app_page_t *page_devinfo_get(void);

/* About page (OS / SDK / APP / SE / WiFi versions) */
app_page_t *page_about_get(void);

/* Network settings page (4G / WiFi switch + connection status) */
app_page_t *page_network_get(void);

/* WiFi AP scan list and connection */
app_page_t *page_wifi_ap_get(void);

/* WiFi connection details (IP / mask / gateway / DNS / MAC) */
app_page_t *page_wifi_info_get(void);

/* Tools page (maintenance mode / memory monitor) */
app_page_t *page_tools_get(void);

/* TMS remote upgrade page */
app_page_t *page_tms_get(void);

/* Is a TMS check running (while running, the page stack is empty and the nav bar back key is used to cancel) */
bool page_tms_is_running(void);

/* Request cancellation of a running TMS check (called by the nav bar back key) */
void page_tms_cancel(void);

/* Print test page */
app_page_t *page_print_get(void);

/* Scan page */
app_page_t *page_scan_get(void);

/* Payment page (local demo flow) */
app_page_t *page_payment_get(void);

/* EMV transaction flow page: amountCents is the amount; cardType is "MSR" / "ICC" / "Contactless";
 * cardData is the track content or ATR/UID. Returns a singleton page; call app_page_push() right after. */
app_page_t *page_emv_open(unsigned long amountCents, const char *cardType, const char *cardData);

/* Hardware test page (magnetic card / IC card / contactless card / scan / e-signature) */
app_page_t *page_test_get(void);

/* E-signature test page (handwritten signature and print) */
app_page_t *page_signature_get(void);

/* Power menu (power off / reboot / cancel) */
app_page_t *page_power_get(void);

/* ---------------- Tamper alarm ---------------- */

/* Tamper alarm page (globally popped up by app_page_dispatch after a periodic check finds a new trigger) */
app_page_t *page_tamper_get(void);

/* Periodic tamper check (called by the main loop): once per minute, posts APP_EV_TAMPER on a new trigger */
void page_tamper_tick(void);

/* Query tamper status immediately: returns true if triggered, *pucType outputs the tamper code (may be NULL) */
bool page_tamper_status(unsigned char *pucType);

#endif /* __APP_PAGES_H__ */
