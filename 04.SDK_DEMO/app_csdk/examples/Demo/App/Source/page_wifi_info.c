/**********************************************************************************************************
 * File Name     : page_wifi_info.c
 * Description   : WiFi connection details (IP / mask / gateway / DNS / MAC)
 *
 *   The data source matches the original UIWifiInfo(), reading each item via Lib_WifiGetLinkAp(),
 *   and each item shows "-" when not connected.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibWifi.h"

#include <string.h>

#define WIFI_INFO_ROW_COUNT     8
#define WIFI_INFO_Y0            12

enum
{
    WIFI_INFO_STATUS = 0,
    WIFI_INFO_SSID,
    WIFI_INFO_IP,
    WIFI_INFO_MASK,
    WIFI_INFO_GATEWAY,
    WIFI_INFO_DNS1,
    WIFI_INFO_DNS2,
    WIFI_INFO_MAC,
};

static const char *s_names[WIFI_INFO_ROW_COUNT] =
{
    "Status", "SSID", "IP Address", "Subnet Mask",
    "Gateway", "DNS 1", "DNS 2", "MAC Address",
};

static lv_obj_t *s_values[WIFI_INFO_ROW_COUNT];

static void wifi_info_read(char *out, unsigned short size, WIFI_PARA_TYPE type)
{
    unsigned short len;

    if (out == NULL || size == 0)
    {
        return;
    }

    memset(out, 0, size);
    len = (unsigned short)(size - 1);

    if (Lib_WifiGetLinkAp((unsigned char *)out, &len, type) != RET_OK)
    {
        out[0] = '\0';
        return;
    }

    if (len >= size)
    {
        len = (unsigned short)(size - 1);
    }

    out[len] = '\0';
}

static void wifi_info_set(int index, const char *text)
{
    if (s_values[index] != NULL)
    {
        lv_label_set_text(s_values[index], LIB_TR((text != NULL && text[0] != '\0') ? text : "-"));
    }
}

static void wifi_info_create(app_page_t *page)
{
    char acBuf[64] = { 0 };
    unsigned char status;
    int i;

    for (i = 0; i < WIFI_INFO_ROW_COUNT; i++)
    {
        s_values[i] = app_row(page->body, WIFI_INFO_Y0 + i * (APP_ROW_H + APP_ROW_GAP), s_names[i]);
    }

    status = Lib_WifiGetApStatus();

    if (status == STA_AP_CONNECTED)
    {
        wifi_info_set(WIFI_INFO_STATUS, "Connected");
    }
    else if (status == STA_AP_CONNECTING)
    {
        wifi_info_set(WIFI_INFO_STATUS, "Connecting");
    }
    else
    {
        wifi_info_set(WIFI_INFO_STATUS, "Disconnected");
    }

    if (status != STA_AP_CONNECTED)
    {
        for (i = WIFI_INFO_SSID; i < WIFI_INFO_ROW_COUNT; i++)
        {
            wifi_info_set(i, "-");
        }
        return;
    }

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_SSID);
    wifi_info_set(WIFI_INFO_SSID, acBuf);

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_LOCAL_IP);
    wifi_info_set(WIFI_INFO_IP, acBuf);

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_NET_MASK);
    wifi_info_set(WIFI_INFO_MASK, acBuf);

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_NET_GATE);
    wifi_info_set(WIFI_INFO_GATEWAY, acBuf);

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_NET_DNS1);
    wifi_info_set(WIFI_INFO_DNS1, acBuf);

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_NET_DNS2);
    wifi_info_set(WIFI_INFO_DNS2, acBuf);

    wifi_info_read(acBuf, sizeof(acBuf), WIFI_PARA_LOCAL_MAC);
    wifi_info_set(WIFI_INFO_MAC, acBuf);
}

static void wifi_info_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < WIFI_INFO_ROW_COUNT; i++)
    {
        s_values[i] = NULL;
    }
}

static app_page_t s_wifiInfoPage =
{
    .id         = "wifi_info",
    .title      = "WiFi DETAILS",
    .on_create  = wifi_info_create,
    .on_event   = NULL,
    .on_destroy = wifi_info_destroy,
    .user_data  = NULL,
};

app_page_t *page_wifi_info_get(void)
{
    return &s_wifiInfoPage;
}
