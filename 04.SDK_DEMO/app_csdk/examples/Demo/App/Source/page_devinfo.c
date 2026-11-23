/**********************************************************************************************************
 * File Name     : page_devinfo.c
 * Description   : Device info page (PN / SN / IMEI / battery / storage / tamper status)
 *
 *   Values are sampled once when entering the page; press ENTER to refresh manually.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibApi.h"

#include <stdio.h>
#include <string.h>

#define DEVINFO_ROW_COUNT       7

enum
{
    DEVINFO_MODEL = 0,
    DEVINFO_SN,
    DEVINFO_IMEI,
    DEVINFO_BATTERY,
    DEVINFO_VOLTAGE,
    DEVINFO_STORAGE,
    DEVINFO_TAMPER,
};

static lv_obj_t *s_values[DEVINFO_ROW_COUNT];

static void devinfo_set(int index, const char *text)
{
    if (index >= 0 && index < DEVINFO_ROW_COUNT && s_values[index] != NULL)
    {
        lv_label_set_text(s_values[index], LIB_TR(text));
    }
}

static void devinfo_refresh(void)
{
    unsigned char aucPN[32] = { 0 };
    unsigned char ucTamper = 0;
    char acBuf[64] = { 0 };
    unsigned int uiTotal = 0;
    unsigned long ulLen = sizeof(uiTotal);
    long lFree = 0;

    /* Model */
    Lib_GetPN(aucPN);
    aucPN[sizeof(aucPN) - 1] = '\0';
    devinfo_set(DEVINFO_MODEL, (char *)aucPN);

    /* Serial number */
    memset(acBuf, 0, sizeof(acBuf));
    if (Lib_ReadSn((unsigned char *)acBuf, sizeof(acBuf) - 1) <= 0)
    {
        devinfo_set(DEVINFO_SN, "N/A");
    }
    else
    {
        devinfo_set(DEVINFO_SN, acBuf);
    }

    /* IMEI */
    memset(acBuf, 0, sizeof(acBuf));
    if (Lib_WlsGetInfo(ID_GPRS_IMEI, acBuf, sizeof(acBuf) - 1) != RET_OK)
    {
        devinfo_set(DEVINFO_IMEI, "N/A");
    }
    else
    {
        devinfo_set(DEVINFO_IMEI, acBuf);
    }

    /* Battery */
    if (Lib_ExtPower())
    {
        devinfo_set(DEVINFO_BATTERY, "Charging");
    }
    else
    {
        snprintf(acBuf, sizeof(acBuf), "%d%%", Lib_BatGetCapacity());
        devinfo_set(DEVINFO_BATTERY, acBuf);
    }

    /* Voltage */
    snprintf(acBuf, sizeof(acBuf), "%lu mV", Lib_BatGetVoltage());
    devinfo_set(DEVINFO_VOLTAGE, acBuf);

    /* File system storage */
    Lib_GetInfo(E_FILE_TOTAL_SIZE, &uiTotal, &ulLen);
    lFree = Lib_FileFreeSize();
    snprintf(acBuf, sizeof(acBuf), "%ld / %u KB", lFree / 1024, uiTotal / 1024);
    devinfo_set(DEVINFO_STORAGE, acBuf);

    /* Tamper status: warning color when triggered, success color when normal */
    if (s_values[DEVINFO_TAMPER] != NULL)
    {
        if (page_tamper_status(&ucTamper))
        {
            snprintf(acBuf, sizeof(acBuf), "%s (0x%02X)", LIB_TR("Triggered"), ucTamper);
            lv_label_set_text(s_values[DEVINFO_TAMPER], acBuf);
            lv_obj_set_style_text_color(s_values[DEVINFO_TAMPER], APP_COLOR_WARN, 0);
        }
        else
        {
            lv_label_set_text(s_values[DEVINFO_TAMPER], LIB_TR("Normal"));
            lv_obj_set_style_text_color(s_values[DEVINFO_TAMPER], APP_COLOR_SUCCESS, 0);
        }
    }
}

static void devinfo_create(app_page_t *page)
{
    static const char *names[DEVINFO_ROW_COUNT] =
    {
        "Model", "Serial No", "IMEI", "Battery", "Voltage", "Storage", "Tamper",
    };
    int i;

    for (i = 0; i < DEVINFO_ROW_COUNT; i++)
    {
        s_values[i] = app_row(page->body,
                              APP_ROW_START_Y + i * (APP_ROW_H + APP_ROW_GAP),
                              names[i]);
    }

    devinfo_refresh();
}

static void devinfo_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < DEVINFO_ROW_COUNT; i++)
    {
        s_values[i] = NULL;
    }
}

static bool devinfo_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param == KB_ENTER)
    {
        devinfo_refresh();
        return true;
    }

    return false;
}

static app_page_t s_devinfoPage =
{
    .id         = "devinfo",
    .title      = "DEVICE INFO",
    .on_create  = devinfo_create,
    .on_event   = devinfo_event,
    .on_destroy = devinfo_destroy,
    .user_data  = NULL,
};

app_page_t *page_devinfo_get(void)
{
    return &s_devinfoPage;
}
