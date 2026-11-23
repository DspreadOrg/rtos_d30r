/**********************************************************************************************************
 * File Name     : page_network.c
 * Description   : Network settings page (4G / WiFi mode switch + connection status)
 *
 *   The mode switching logic is the same as the original UINetModeCfg():
 *     SetNetWorkMode() -> GPRS: Lib_WifiNetworkDown()
 *                      -> WiFi: Lib_WifiNetworkUp() + Lib_WifiSetAutoConnectFlag(TRUE)
 *   The switch writes parameters and syncs them to the SDK, taking a few seconds, so the hint is flushed to the screen first.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "ui_status_bar.h"

#include "LibNetwork.h"
#include "LibSysapi.h"
#include "LibWifi.h"
#include "LibGprs.h"
#include "LibParamManage.h"

#include <stdio.h>
#include <string.h>

#define NET_MODE_ITEM_COUNT     2
#define NET_REFRESH_MS          1000    /* Auto refresh period for status / IP info while on this page */
#define NET_MODE_BTN_W          216
#define NET_MODE_BTN_H          84
#define NET_MODE_BTN_Y          16
#define NET_MODE_BTN_X0         16
#define NET_MODE_BTN_X1         248

#define NET_FOCUS_GPRS          0
#define NET_FOCUS_WIFI          1
#define NET_FOCUS_RECONNECT     2
#define NET_FOCUS_APLIST        3
#define NET_FOCUS_DETAIL        4
#define NET_FOCUS_COUNT         5

#define NET_ROW_Y0              120
#define NET_RECONNECT_Y         440

static const char *s_modeNames[NET_MODE_ITEM_COUNT] = { "4G", "WiFi" };
static const unsigned char s_modeValues[NET_MODE_ITEM_COUNT] = { NET_MODE_GPRS, NET_MODE_WIFI };

enum
{
    NET_STATUS = 0,
    NET_SSID,
    NET_IP,
    NET_MAC,
    NET_ROW_COUNT,
};

static lv_obj_t *s_modeButtons[NET_MODE_ITEM_COUNT];
static lv_obj_t *s_values[NET_ROW_COUNT];
static lv_obj_t *s_reconnectButton = NULL;
static lv_obj_t *s_apListButton = NULL;
static lv_obj_t *s_detailButton = NULL;
static lv_timer_t *s_refreshTimer = NULL;
static int s_focus = NET_FOCUS_GPRS;

static void network_set(int index, const char *text)
{
    if (index >= 0 && index < NET_ROW_COUNT && s_values[index] != NULL)
    {
        const char *current = lv_label_get_text(s_values[index]);
        const char *shown = LIB_TR(text);

        /* Do not redraw when the content is unchanged, avoiding useless screen refreshes on timer ticks */
        if ((current == NULL) || (strcmp(current, shown) != 0))
        {
            lv_label_set_text(s_values[index], shown);
        }
    }
}

static void network_apply_focus(void)
{
    int i;

    for (i = 0; i < NET_MODE_ITEM_COUNT; i++)
    {
        if (s_modeButtons[i] == NULL)
        {
            continue;
        }

        if (s_focus == i)
        {
            lv_obj_set_style_border_width(s_modeButtons[i], 3, 0);
            lv_obj_set_style_border_color(s_modeButtons[i], APP_COLOR_TEXT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_modeButtons[i], 0, 0);
        }
    }

    if (s_reconnectButton != NULL)
    {
        if (s_focus == NET_FOCUS_RECONNECT)
        {
            lv_obj_set_style_border_width(s_reconnectButton, 3, 0);
            lv_obj_set_style_border_color(s_reconnectButton, APP_COLOR_TEXT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_reconnectButton, 0, 0);
        }
    }

    if (s_apListButton != NULL)
    {
        if (s_focus == NET_FOCUS_APLIST)
        {
            lv_obj_set_style_border_width(s_apListButton, 3, 0);
            lv_obj_set_style_border_color(s_apListButton, APP_COLOR_TEXT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_apListButton, 0, 0);
        }
    }

    if (s_detailButton != NULL)
    {
        if (s_focus == NET_FOCUS_DETAIL)
        {
            lv_obj_set_style_border_width(s_detailButton, 3, 0);
            lv_obj_set_style_border_color(s_detailButton, APP_COLOR_TEXT, 0);
        }
        else
        {
            lv_obj_set_style_border_width(s_detailButton, 0, 0);
        }
    }
}

static void network_refresh(void)
{
    unsigned char mode = NET_MODE_WIFI;
    char acBuf[64] = { 0 };
    int status = 0;

    if (Lib_ParamGetU8(E_PARAM_NETWORK_MODE, &mode) != RET_OK)
    {
        mode = NET_MODE_WIFI;
    }

    /* Highlight the mode buttons */
    {
        int i;

        for (i = 0; i < NET_MODE_ITEM_COUNT; i++)
        {
            if (s_modeButtons[i] == NULL)
            {
                continue;
            }

            if (mode == s_modeValues[i])
            {
                lv_obj_set_style_bg_color(s_modeButtons[i], APP_COLOR_ACCENT, 0);
                lv_obj_set_style_bg_opa(s_modeButtons[i], LV_OPA_COVER, 0);
            }
            else
            {
                lv_obj_set_style_bg_color(s_modeButtons[i], APP_COLOR_CARD, 0);
                lv_obj_set_style_bg_opa(s_modeButtons[i], LV_OPA_COVER, 0);
            }
        }
    }

    if (mode == NET_MODE_GPRS)
    {
        int initStatus = Lib_WlsGetInitStatus();

        /* The 4G status also follows the module state; the periodic refresh gives the latest hint */
        if (initStatus == WLS_NOSIM)
        {
            network_set(NET_STATUS, "No SIM");
        }
        else if ((initStatus != WLS_OK) && (initStatus != WLS_ATTACHED))
        {
            network_set(NET_STATUS, "No service");
        }
        else
        {
            network_set(NET_STATUS, "Connected");
        }

        network_set(NET_SSID, "-");
        network_set(NET_IP, "-");
        network_set(NET_MAC, "-");
        return;
    }

    status = Lib_WifiGetApStatus();

    switch (status)
    {
    case STA_AP_CONNECTED:
        network_set(NET_STATUS, "Connected");
        break;

    case STA_AP_CONNECTING:
        network_set(NET_STATUS, "Connecting");
        break;

    case STA_AP_DISCONNECTED:
        network_set(NET_STATUS, "Disconnected");
        break;

    default:
        network_set(NET_STATUS, "Unknown");
        break;
    }

    if (status != STA_AP_CONNECTED)
    {
        network_set(NET_SSID, "-");
        network_set(NET_IP, "-");
        network_set(NET_MAC, "-");
        return;
    }

    memset(acBuf, 0, sizeof(acBuf));
    {
        unsigned short len = (unsigned short)(sizeof(acBuf) - 1);

        if (Lib_WifiGetLinkAp((unsigned char *)acBuf, &len, WIFI_PARA_SSID) != RET_OK)
        {
            acBuf[0] = '\0';
        }
        else if (len < sizeof(acBuf))
        {
            acBuf[len] = '\0';
        }
    }
    network_set(NET_SSID, (acBuf[0] != '\0') ? acBuf : "-");

    memset(acBuf, 0, sizeof(acBuf));
    {
        unsigned short len = (unsigned short)(sizeof(acBuf) - 1);

        if (Lib_WifiGetLinkAp((unsigned char *)acBuf, &len, WIFI_PARA_LOCAL_IP) != RET_OK)
        {
            acBuf[0] = '\0';
        }
        else if (len < sizeof(acBuf))
        {
            acBuf[len] = '\0';
        }
    }
    network_set(NET_IP, (acBuf[0] != '\0') ? acBuf : "-");

    memset(acBuf, 0, sizeof(acBuf));
    {
        unsigned short len = (unsigned short)(sizeof(acBuf) - 1);

        if (Lib_WifiGetLinkAp((unsigned char *)acBuf, &len, WIFI_PARA_LOCAL_MAC) != RET_OK)
        {
            acBuf[0] = '\0';
        }
        else if (len < sizeof(acBuf))
        {
            acBuf[len] = '\0';
        }
    }
    network_set(NET_MAC, (acBuf[0] != '\0') ? acBuf : "-");
}

static void network_switch_mode(int index)
{
    unsigned char mode;

    if (index < 0 || index >= NET_MODE_ITEM_COUNT)
    {
        return;
    }

    mode = s_modeValues[index];

    network_set(NET_STATUS, "Switching...");
    lv_refr_now(NULL);

    if (Lib_ParamSet(E_PARAM_NETWORK_MODE, &mode, sizeof(mode)) != RET_OK)
    {
        network_set(NET_STATUS, "Set mode failed");
        return;
    }

    Lib_ParamSave();

    Lib_SetInfo(E_NET_MODE, &mode, sizeof(mode));

    /* Same as the original UINetModeCfg(): switching to 4G turns WiFi off, switching to WiFi turns it on and reconnects automatically */
    if (mode == NET_MODE_GPRS)
    {
        Lib_WifiNetworkDown();
    }
    else
    {
        Lib_WifiNetworkUp();
        Lib_WifiSetAutoConnectFlag(TRUE);
    }

    s_focus = index;
    network_apply_focus();
    network_refresh();

    /* Switch the status bar icon to the new network signal style immediately; the status bar timer refreshes it periodically afterwards */
    app_status_bar_refresh_signal();
}

static void network_reconnect(void)
{
    network_set(NET_STATUS, "Reconnecting...");
    lv_refr_now(NULL);

    Lib_WifiNetworkDown();

    Lib_WifiNetworkUp();
    Lib_WifiSetAutoConnectFlag(TRUE);

    network_refresh();
    app_status_bar_refresh_signal();
}

static void network_mode_clicked_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    network_switch_mode(index);
}

static void network_reconnect_cb(lv_event_t *e)
{
    (void)e;

    network_reconnect();
}

static void network_open_aplist(void)
{
    app_page_t *page = page_wifi_ap_get();

    if (page != NULL)
    {
        app_page_push(page);
    }
}

static void network_aplist_cb(lv_event_t *e)
{
    (void)e;

    network_open_aplist();
}

static void network_detail_cb(lv_event_t *e)
{
    app_page_t *page = page_wifi_info_get();
    (void)e;

    if (page != NULL)
    {
        app_page_push(page);
    }
}

static lv_obj_t *network_mode_button(lv_obj_t *parent, int index)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;
    lv_coord_t x = (index == 0) ? NET_MODE_BTN_X0 : NET_MODE_BTN_X1;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, NET_MODE_BTN_Y);
    lv_obj_set_size(btn, NET_MODE_BTN_W, NET_MODE_BTN_H);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ICON_BG, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, LIB_TR(s_modeNames[index]));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(28), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, network_mode_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return btn;
}

static lv_obj_t *network_create_button(lv_obj_t *parent, lv_coord_t y, const char *text)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, APP_ROW_X, y);
    lv_obj_set_size(btn, APP_ROW_W, 60);
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

static void network_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    network_refresh();
}

static void network_create(app_page_t *page)
{
    static const char *names[NET_ROW_COUNT] =
    {
        "Status", "SSID", "IP Address", "MAC Address",
    };
    lv_obj_t *tip = NULL;
    int i;

    for (i = 0; i < NET_MODE_ITEM_COUNT; i++)
    {
        s_modeButtons[i] = network_mode_button(page->body, i);
    }

    for (i = 0; i < NET_ROW_COUNT; i++)
    {
        s_values[i] = app_row(page->body, NET_ROW_Y0 + i * (APP_ROW_H + APP_ROW_GAP), names[i]);
    }

    s_reconnectButton = network_create_button(page->body, NET_RECONNECT_Y, LIB_TR("Reconnect WiFi"));
    if (s_reconnectButton != NULL)
    {
        lv_obj_add_event_cb(s_reconnectButton, network_reconnect_cb, LV_EVENT_CLICKED, NULL);
    }

    s_apListButton = network_create_button(page->body, NET_RECONNECT_Y + 76, LIB_TR("WiFi AP List"));
    if (s_apListButton != NULL)
    {
        lv_obj_add_event_cb(s_apListButton, network_aplist_cb, LV_EVENT_CLICKED, NULL);
    }

    s_detailButton = network_create_button(page->body, NET_RECONNECT_Y + 152, LIB_TR("WiFi Details"));
    if (s_detailButton != NULL)
    {
        lv_obj_add_event_cb(s_detailButton, network_detail_cb, LV_EVENT_CLICKED, NULL);
    }

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Tap 4G / WiFi to switch. Enter applies the focused item."));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(14), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_focus = NET_FOCUS_GPRS;
    network_apply_focus();
    network_refresh();

    /* Refresh periodically while on this page, so WiFi auto-connect / 4G registration changes need no page re-entry */
    if (s_refreshTimer != NULL)
    {
        lv_timer_del(s_refreshTimer);
        s_refreshTimer = NULL;
    }

    s_refreshTimer = lv_timer_create(network_timer_cb, NET_REFRESH_MS, NULL);
}

static void network_destroy(app_page_t *page)
{
    int i;

    (void)page;

    if (s_refreshTimer != NULL)
    {
        lv_timer_del(s_refreshTimer);
        s_refreshTimer = NULL;
    }

    for (i = 0; i < NET_MODE_ITEM_COUNT; i++)
    {
        s_modeButtons[i] = NULL;
    }

    for (i = 0; i < NET_ROW_COUNT; i++)
    {
        s_values[i] = NULL;
    }

    s_reconnectButton = NULL;
    s_apListButton = NULL;
    s_detailButton = NULL;
}

static bool network_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    switch (ev->param)
    {
    case KB_UP:
        s_focus = (s_focus + NET_FOCUS_COUNT - 1) % NET_FOCUS_COUNT;
        network_apply_focus();
        return true;

    case KB_DOWN:
        s_focus = (s_focus + 1) % NET_FOCUS_COUNT;
        network_apply_focus();
        return true;

    case KB_ENTER:
        if (s_focus == NET_FOCUS_RECONNECT)
        {
            network_reconnect();
        }
        else if (s_focus == NET_FOCUS_APLIST)
        {
            network_open_aplist();
        }
        else if (s_focus == NET_FOCUS_DETAIL && s_detailButton != NULL)
        {
            lv_event_send(s_detailButton, LV_EVENT_CLICKED, NULL);
        }
        else
        {
            network_switch_mode(s_focus);
        }
        return true;

    case KB_DIGIT_ONE:
        network_switch_mode(NET_FOCUS_GPRS);
        return true;

    case KB_DIGIT_TWO:
        network_switch_mode(NET_FOCUS_WIFI);
        return true;

    default:
        break;
    }

    return false;
}

static app_page_t s_networkPage =
{
    .id         = "network",
    .title      = "NETWORK",
    .on_create  = network_create,
    .on_event   = network_event,
    .on_destroy = network_destroy,
    .user_data  = NULL,
};

app_page_t *page_network_get(void)
{
    return &s_networkPage;
}
