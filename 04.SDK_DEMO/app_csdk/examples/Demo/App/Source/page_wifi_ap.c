/**********************************************************************************************************
 * File Name     : page_wifi_ap.c
 * Description   : WiFi AP scan list, connect and forget network
 *
 *   References the original UIWifiNetList() / UIWifiSelectNet() / UIWifiLinkAct():
 *     Lib_WifiNetworkScan() -> poll Lib_WifiGetAP() to get the list
 *     For an encrypted AP, enter the password with the generic keyboard, then Lib_WifiConnectAp()
 *     Poll GetLinkWifi() for the connection result and save the SSID on success
 *     Forget calls Lib_WifiSetInfo(NET_WIFI_SETTING_DEL_SSID) to delete the saved network
 *
 *   Same as the original implementation, scanning/connecting run synchronously in the LVGL thread
 *   (the original blocks too) and the hint is flushed to the screen first so the UI does not look frozen.
 **********************************************************************************************************/
#include "global.h"
#include "app_pages.h"
#include "app_ui.h"
#include "app_keyboard.h"
#include "LibWifi.h"
#include "LibNetwork.h"
#include "LibParamManage.h"

#include <stdio.h>
#include <string.h>

/* Implemented in WiFiConfig.c without an exported header declaration; declared here to match the short return value */
extern short GetLinkWifi(WIFI_INF *wifiInfo);

#define AP_MAX_COUNT            16
#define AP_ROW_H                76
#define AP_ROW_GAP              8
#define AP_LIST_Y0              56
#define AP_LIST_H               (AP_BTN_Y - AP_LIST_Y0 - 12)
#define AP_ROW_W                (APP_ROW_W - 6)
#define AP_ICON_SIZE            26
#define AP_BTN_Y                566
#define AP_BTN_H                52
#define AP_SCAN_WAIT_MS         10000
#define AP_CONNECT_WAIT_MS      20000
#define AP_PWD_MAX_LEN          32
#define AP_REFRESH_MS           1000    /* Connection status refresh period while on the list page */

static WIFI_INF s_aps[AP_MAX_COUNT];
static int s_apCount = 0;
static int s_focus = 0;

static app_page_t *s_page = NULL;
static lv_obj_t *s_statusLabel = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_rows[AP_MAX_COUNT];
static lv_obj_t *s_icons[AP_MAX_COUNT];
static lv_obj_t *s_stateLabels[AP_MAX_COUNT];
static bool s_connectedFlags[AP_MAX_COUNT];
static lv_timer_t *s_timer = NULL;
static lv_obj_t *s_rescanBtn = NULL;
static lv_obj_t *s_forgetBtn = NULL;
static int s_rowCount = 0;

static void ap_status(const char *text)
{
    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR(text));
    }
}

static int ap_focus_count(void)
{
    return s_rowCount + 2;
}

static void ap_apply_focus(void)
{
    int i;

    for (i = 0; i < s_rowCount; i++)
    {
        if (s_rows[i] != NULL)
        {
            lv_obj_set_style_border_width(s_rows[i], (i == s_focus) ? 3 : 0, 0);
            lv_obj_set_style_border_color(s_rows[i], APP_COLOR_ACCENT, 0);
        }
    }

    if (s_rescanBtn != NULL)
    {
        lv_obj_set_style_border_width(s_rescanBtn, (s_focus == s_rowCount) ? 3 : 0, 0);
        lv_obj_set_style_border_color(s_rescanBtn, APP_COLOR_TEXT, 0);
    }

    if (s_forgetBtn != NULL)
    {
        lv_obj_set_style_border_width(s_forgetBtn, (s_focus == s_rowCount + 1) ? 3 : 0, 0);
        lv_obj_set_style_border_color(s_forgetBtn, APP_COLOR_TEXT, 0);
    }

    /* Scroll the focused row into view when the list exceeds the visible area */
    if ((s_focus >= 0) && (s_focus < s_rowCount) && (s_rows[s_focus] != NULL))
    {
        lv_obj_scroll_to_view(s_rows[s_focus], LV_ANIM_OFF);
    }
}

/* Refresh each row from the actual connection state: CONNECTED mark and icon color */
static void ap_refresh_states(void)
{
    WIFI_INF linked;
    char acSsid[50] = { 0 };
    int i;

    memset(&linked, 0, sizeof(linked));
    if (GetLinkWifi(&linked) == RET_OK)
    {
        strncpy(acSsid, (char *)linked.m_ausSSID, sizeof(acSsid) - 1);
    }

    for (i = 0; i < s_rowCount; i++)
    {
        bool connected = (acSsid[0] != '\0') &&
                         (strcmp(acSsid, (char *)s_aps[i].m_ausSSID) == 0);

        if (s_connectedFlags[i] == connected)
        {
            continue;
        }

        s_connectedFlags[i] = connected;

        if (s_stateLabels[i] != NULL)
        {
            if (connected)
            {
                lv_obj_clear_flag(s_stateLabels[i], LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(s_stateLabels[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (s_icons[i] != NULL)
        {
            app_wifi_signal_set_color(s_icons[i],
                                      connected ? APP_COLOR_SUCCESS : APP_COLOR_ACCENT);
        }
    }
}

static void ap_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    ap_refresh_states();
}

/* ---------------- Scan ---------------- */

static int ap_scan(void)
{
    unsigned int start;

    ap_status("Scanning...");
    lv_refr_now(NULL);

    Lib_WifiNetworkScan();

    start = Lib_SysGetTimer();
    s_apCount = 0;

    for (;;)
    {
        unsigned short count = 0;

        memset(s_aps, 0, sizeof(s_aps));
        Lib_WifiGetAP(s_aps, &count, AP_MAX_COUNT);

        if (count > 0)
        {
            s_apCount = (count > AP_MAX_COUNT) ? AP_MAX_COUNT : (int)count;
            break;
        }

        if ((Lib_SysGetTimer() - start) >= AP_SCAN_WAIT_MS)
        {
            s_apCount = 0;
            break;
        }

        Lib_DelayMs(200);
    }

    return s_apCount;
}

//-----------------------------------------------------------------------------
// Name    : GetLinkWifi
// Func    : Get connected WiFi AP infos
// Paras   :
// Ret     :
// Desc    :
//-----------------------------------------------------------------------------
short GetLinkWifi(WIFI_INF* wifiInfo)
{
    if (NULL == wifiInfo)
        return RET_ERR;

    TRACE("GetLinkWifi");

    unsigned short paraLen = 0;
    unsigned char status = Lib_WifiGetApStatus();
    TRACE("GetLinkWifi status : %d ", status);
    if (STA_AP_CONNECTED == status)
    {
        Lib_WifiGetLinkAp(wifiInfo->m_ausSSID, &paraLen, WIFI_PARA_SSID);
        Lib_WifiGetLinkAp(wifiInfo->m_aucPass, &paraLen, WIFI_PARA_PWD);
        TRACE("GetLinkWifi OK");
        return RET_OK;
    }
    else if (STA_AP_DISCONNECTED == status)
    {
        TRACE("GetLinkWifi Fail");
        return RET_ERR_TIMEOUT;
    }
    return RET_ERR;
}
/* ---------------- Connect / Forget ---------------- */

static int ap_connect(int index, const char *password)
{
    WIFI_INF linked;
    unsigned int start;
    char acMsg[64] = { 0 };
    char *ssid = (char *)s_aps[index].m_ausSSID;
    int rc = RET_ERR;

    Lib_LangFormat(acMsg, sizeof(acMsg), "Connecting to %.20s...", ssid);
    ap_status(acMsg);
    lv_refr_now(NULL);

    /* Encryption type 6 means an open network, same as the original UIWifiLinkActNoCofig() */
    if (s_aps[index].m_aucEncryptType == 6)
    {
        Lib_WifiConnectAp((unsigned char *)"6", (unsigned char *)"NONE", ssid, (unsigned char *)password);
    }
    else
    {
        Lib_WifiConnectAp((unsigned char *)"WPA", (unsigned char *)"NONE", ssid, (unsigned char *)password);
    }

    start = Lib_SysGetTimer();
    for (;;)
    {
        memset(&linked, 0, sizeof(linked));

        rc = GetLinkWifi(&linked);
        if (rc == RET_OK)
        {
            break;
        }

        if ((Lib_SysGetTimer() - start) >= AP_CONNECT_WAIT_MS)
        {
            rc = RET_ERR_TIMEOUT;
            break;
        }

        Lib_DelayMs(200);
    }

    if (rc == RET_OK)
    {
        Lib_ParamGetStr(E_PARAM_WIFI_SSID, ssid, sizeof(ssid));
        Lib_WifiSetAutoConnectFlag(TRUE);
        ap_status("Connected");

        /* Refresh the CONNECTED marks in the list immediately after switching to another AP */
        ap_refresh_states();
        return RET_OK;
    }

    ap_status("Connect failed");
    return rc;
}

static void ap_forget(int index)
{
    unsigned short len;

    if (index < 0 || index >= s_apCount)
    {
        return;
    }

    len = (unsigned short)strlen((char *)s_aps[index].m_ausSSID);
    if (len == 0)
    {
        return;
    }

    Lib_WifiSetInfo(NET_WIFI_SETTING_DEL_SSID, s_aps[index].m_ausSSID, len);

    ap_status("Network forgotten");

    /* Refresh the marks from the actual connection state after forgetting a network */
    ap_refresh_states();
}

/* ---------------- Password input ---------------- */

static void ap_pwd_done_cb(const char *text, void *user_data)
{
    int index = (int)(intptr_t)user_data;

    ap_connect(index, (text != NULL) ? text : "");
}

static void ap_pwd_cancel_cb(void *user_data)
{
    (void)user_data;

    ap_status("Cancelled");
}

static void ap_start_input(int index)
{
    char acTitle[64] = { 0 };

    Lib_LangFormat(acTitle, sizeof(acTitle), "Password: %.24s", (char *)s_aps[index].m_ausSSID);
    app_keyboard_open(acTitle, "", true, AP_PWD_MAX_LEN,
                      ap_pwd_done_cb, ap_pwd_cancel_cb, (void *)(intptr_t)index);
}

/* ---------------- List UI ---------------- */

static void ap_row_clicked_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index < 0 || index >= s_apCount)
    {
        return;
    }

    s_focus = index;
    ap_apply_focus();

    if (s_aps[index].m_aucEncryptType == 6)
    {
        ap_connect(index, "");
    }
    else
    {
        ap_start_input(index);
    }
}

static void ap_rescan_cb(lv_event_t *e)
{
    (void)e;

    if (s_page != NULL)
    {
        lv_obj_clean(s_page->body);
        s_rowCount = 0;
        s_statusLabel = NULL;
        s_list = NULL;
        s_rescanBtn = NULL;
        s_forgetBtn = NULL;
        memset(s_rows, 0, sizeof(s_rows));
        memset(s_icons, 0, sizeof(s_icons));
        memset(s_stateLabels, 0, sizeof(s_stateLabels));
        memset(s_connectedFlags, 0, sizeof(s_connectedFlags));
        s_page->on_create(s_page);
    }
}

static void ap_forget_cb(lv_event_t *e)
{
    (void)e;

    if (s_focus < s_rowCount)
    {
        ap_forget(s_focus);
    }
}

static void ap_build_list(app_page_t *page)
{
    int i;
    int rows = (s_apCount > AP_MAX_COUNT) ? AP_MAX_COUNT : s_apCount;

    s_statusLabel = lv_label_create(page->body);
    lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_statusLabel, app_font(18), 0);
    lv_obj_set_pos(s_statusLabel, APP_ROW_X, 12);
    lv_label_set_text_fmt(s_statusLabel, "%d AP found", s_apCount);

    /* Scrollable list container: scroll up/down when the AP count exceeds the visible rows */
    s_list = lv_obj_create(page->body);
    if (s_list != NULL)
    {
        lv_obj_remove_style_all(s_list);
        lv_obj_set_pos(s_list, APP_ROW_X, AP_LIST_Y0);
        lv_obj_set_size(s_list, APP_ROW_W, AP_LIST_H);
        lv_obj_set_style_pad_all(s_list, 0, 0);
        lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(s_list, 4, LV_PART_SCROLLBAR);
        lv_obj_set_style_radius(s_list, 2, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_color(s_list, APP_COLOR_SUB, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(s_list, LV_OPA_50, LV_PART_SCROLLBAR);
        lv_obj_clear_flag(s_list, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    }

    s_rowCount = 0;
    for (i = 0; i < rows; i++)
    {
        lv_obj_t *row = NULL;
        lv_obj_t *name = NULL;
        lv_obj_t *icon = NULL;
        lv_obj_t *state = NULL;

        if (s_list == NULL)
        {
            break;
        }

        row = app_card(s_list, 0, i * (AP_ROW_H + AP_ROW_GAP), AP_ROW_W, AP_ROW_H);
        if (row == NULL)
        {
            break;
        }

        name = lv_label_create(row);
        lv_label_set_text(name, (char *)s_aps[i].m_ausSSID);
        lv_obj_set_width(name, 250);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(name, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(name, app_font(20), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 16, 0);

        /* Show signal strength with an Android-style fan icon on the right instead of the old OPEN/LOCK text */
        icon = app_wifi_signal_icon(row, AP_ICON_SIZE, APP_COLOR_ACCENT);
        if (icon != NULL)
        {
            app_wifi_signal_set_level(icon,
                                      app_signal_level_from_raw((int)s_aps[i].m_aucSignalLevel));
            lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -16, 0);
        }

        /* Create the CONNECTED mark hidden; ap_refresh_states() shows it per the actual state */
        state = lv_label_create(row);
        lv_label_set_text(state, LIB_TR("CONNECTED"));
        lv_obj_set_style_text_color(state, APP_COLOR_SUCCESS, 0);
        lv_obj_set_style_text_font(state, app_font(14), 0);
        lv_obj_align(state, LV_ALIGN_RIGHT_MID, -(AP_ICON_SIZE + 26), 0);
        lv_obj_add_flag(state, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(row, ap_row_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        s_rows[i] = row;
        s_icons[i] = icon;
        s_stateLabels[i] = state;
        s_connectedFlags[i] = false;
        s_rowCount++;
    }

    s_focus = 0;
    ap_apply_focus();
    ap_refresh_states();
}

static lv_obj_t *ap_create_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t w, const char *text, lv_color_t color)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, AP_BTN_H);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(18), 0);
    lv_obj_center(label);

    return btn;
}

static void ap_create(app_page_t *page)
{
    s_page = page;
    s_statusLabel = NULL;
    s_list = NULL;
    s_rescanBtn = NULL;
    s_forgetBtn = NULL;
    s_rowCount = 0;
    memset(s_rows, 0, sizeof(s_rows));
    memset(s_icons, 0, sizeof(s_icons));
    memset(s_stateLabels, 0, sizeof(s_stateLabels));
    memset(s_connectedFlags, 0, sizeof(s_connectedFlags));

    if (ap_scan() > 0)
    {
        ap_build_list(page);
    }
    else
    {
        s_statusLabel = lv_label_create(page->body);
        lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_WARN, 0);
        lv_obj_set_style_text_font(s_statusLabel, app_font(18), 0);
        lv_obj_set_pos(s_statusLabel, APP_ROW_X, 16);
        lv_label_set_text(s_statusLabel, LIB_TR("No AP found"));
    }

    s_rescanBtn = ap_create_button(page->body, APP_ROW_X, AP_BTN_Y, 216, LIB_TR("Rescan"), APP_COLOR_ACCENT);
    if (s_rescanBtn != NULL)
    {
        lv_obj_add_event_cb(s_rescanBtn, ap_rescan_cb, LV_EVENT_CLICKED, NULL);
    }

    s_forgetBtn = ap_create_button(page->body, APP_ROW_X + 232, AP_BTN_Y, 216, LIB_TR("Forget"), APP_COLOR_WARN);
    if (s_forgetBtn != NULL)
    {
        lv_obj_add_event_cb(s_forgetBtn, ap_forget_cb, LV_EVENT_CLICKED, NULL);
    }

    /* Periodically refresh the connection state: the list updates live on connect / disconnect / switching AP */
    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    s_timer = lv_timer_create(ap_timer_cb, AP_REFRESH_MS, NULL);
}

static void ap_destroy(app_page_t *page)
{
    (void)page;

    app_keyboard_close();

    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    s_page = NULL;
    s_statusLabel = NULL;
    s_list = NULL;
    s_rescanBtn = NULL;
    s_forgetBtn = NULL;
    s_rowCount = 0;
    memset(s_rows, 0, sizeof(s_rows));
    memset(s_icons, 0, sizeof(s_icons));
    memset(s_stateLabels, 0, sizeof(s_stateLabels));
    memset(s_connectedFlags, 0, sizeof(s_connectedFlags));
}

static bool ap_event(app_page_t *page, const app_event_t *ev)
{
    int count;

    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    /* Hand key presses to the soft keyboard while it is open */
    if (app_keyboard_is_open())
    {
        return false;
    }

    count = ap_focus_count();
    if (count <= 2)
    {
        return false;
    }

    switch (ev->param)
    {
    case KB_UP:
        s_focus = (s_focus + count - 1) % count;
        ap_apply_focus();
        return true;

    case KB_DOWN:
        s_focus = (s_focus + 1) % count;
        ap_apply_focus();
        return true;

    case KB_ENTER:
        if (s_focus < s_rowCount)
        {
            if (s_aps[s_focus].m_aucEncryptType == 6)
            {
                ap_connect(s_focus, "");
            }
            else
            {
                ap_start_input(s_focus);
            }
        }
        else if (s_focus == s_rowCount && s_rescanBtn != NULL)
        {
            lv_event_send(s_rescanBtn, LV_EVENT_CLICKED, NULL);
        }
        else if (s_forgetBtn != NULL)
        {
            lv_event_send(s_forgetBtn, LV_EVENT_CLICKED, NULL);
        }
        return true;

    case KB_CLEAR:
        if (s_focus < s_rowCount)
        {
            ap_forget(s_focus);
        }
        return true;

    default:
        break;
    }

    return false;
}

static app_page_t s_wifiApPage =
{
    .id         = "wifi_ap",
    .title      = "WiFi NETWORKS",
    .on_create  = ap_create,
    .on_event   = ap_event,
    .on_destroy = ap_destroy,
    .user_data  = NULL,
};

app_page_t *page_wifi_ap_get(void)
{
    return &s_wifiApPage;
}
