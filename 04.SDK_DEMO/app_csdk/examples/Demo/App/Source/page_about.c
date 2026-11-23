/**********************************************************************************************************
 * File Name     : page_about.c
 * Description   : About page (OS / SDK / APP / SE / WiFi versions)
 **********************************************************************************************************/

#include "version.h"
#include "app_pages.h"
#include "app_ui.h"
#include "LibSysapi.h"
#include "LibWifi.h"

#include <string.h>

#define ABOUT_ROW_COUNT         5

enum
{
    ABOUT_OS = 0,
    ABOUT_SDK,
    ABOUT_APP,
    ABOUT_SE,
    ABOUT_WIFI,
};

static lv_obj_t *s_values[ABOUT_ROW_COUNT];

static void about_set(int index, const char *text)
{
    if (index >= 0 && index < ABOUT_ROW_COUNT && s_values[index] != NULL)
    {
        lv_label_set_text(s_values[index], LIB_TR(text));
    }
}

static void about_refresh(void)
{
    char acBuf[64] = { 0 };
    unsigned short usLen = 0;

    Lib_GetVersion((unsigned char *)acBuf, sizeof(acBuf) - 1, 0);
    about_set(ABOUT_OS, acBuf);

    memset(acBuf, 0, sizeof(acBuf));
    Lib_GetVersion((unsigned char *)acBuf, sizeof(acBuf) - 1, 1);
    about_set(ABOUT_SDK, acBuf);

    memset(acBuf, 0, sizeof(acBuf));
    memcpy(acBuf, INFO_DEV_SVER, sizeof(INFO_DEV_SVER));
    about_set(ABOUT_APP, acBuf);

    memset(acBuf, 0, sizeof(acBuf));
    Lib_GetVersion((unsigned char *)acBuf, sizeof(acBuf) - 1, 2);
    about_set(ABOUT_SE, acBuf);

    memset(acBuf, 0, sizeof(acBuf));
    usLen = sizeof(acBuf) - 1;
    if (Lib_WifiGetVersion(acBuf, &usLen) != RET_OK)
    {
        about_set(ABOUT_WIFI, "N/A");
    }
    else
    {
        acBuf[(usLen < sizeof(acBuf)) ? usLen : (sizeof(acBuf) - 1)] = '\0';
        about_set(ABOUT_WIFI, acBuf);
    }
}

static void about_create(app_page_t *page)
{
    static const char *names[ABOUT_ROW_COUNT] =
    {
        "OS", "SDK", "Application", "Security", "WiFi Module",
    };
    int i;

    for (i = 0; i < ABOUT_ROW_COUNT; i++)
    {
        s_values[i] = app_row(page->body,
                              APP_ROW_START_Y + i * (APP_ROW_H + APP_ROW_GAP),
                              names[i]);
    }

    about_refresh();
}

static void about_destroy(app_page_t *page)
{
    int i;

    (void)page;

    for (i = 0; i < ABOUT_ROW_COUNT; i++)
    {
        s_values[i] = NULL;
    }
}

static bool about_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param == KB_ENTER)
    {
        about_refresh();
        return true;
    }

    return false;
}

static app_page_t s_aboutPage =
{
    .id         = "about",
    .title      = "ABOUT",
    .on_create  = about_create,
    .on_event   = about_event,
    .on_destroy = about_destroy,
    .user_data  = NULL,
};

app_page_t *page_about_get(void)
{
    return &s_aboutPage;
}
