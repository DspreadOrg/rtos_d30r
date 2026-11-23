/**********************************************************************************************************
 * File Name     : app_main.c
 * Description   : Application entry: event-driven + page stack main loop
 *
 *   Replaces the original LvHomeMenu() (synchronous blocking menu model).
 *   Keys are captured by the keypad indev and converted into app events via LV_EVENT_KEY in app_page.c.
 **********************************************************************************************************/

#include "app_main.h"
#include "app_event.h"
#include "app_page.h"
#include "app_pages.h"
#include "app_power.h"
#include "app_ui.h"
#include "ui_nav_bar.h"
#include "ui_status_bar.h"
#include "LibApi.h" /* LibLvgl.h is already included; do not include it again */
#include "LibLanguage.h"
#include "LibOsi.h"
#include "LibParamManage.h"
#include "LibSysapi.h"

#if defined(MQTT_SUPPORT)
#include "Mqtt.h"
#endif

#define APP_LOOP_INTERVAL_MS  10

#if defined(MQTT_SUPPORT)
static void app_mqtt_thread(void *arg)
{
    (void)arg;
    MqttTaskThread();
}
#endif

static void app_network_service(void)
{
    unsigned char mode = NET_MODE_WIFI;
    if (Lib_ParamGetU8(E_PARAM_NETWORK_MODE, &mode) != RET_OK)
    {
        mode = NET_MODE_WIFI;
    }

    Lib_SetInfo(E_NET_MODE, &mode, sizeof(mode));

    Lib_WlsSetAirplaneMode(1);
    Lib_WifiNetworkDown();

    if (mode == NET_MODE_WIFI)
    {
        Lib_WifiNetworkUp();
        Lib_WifiSetAutoConnectFlag(TRUE);
    }

    Lib_WlsSetAirplaneMode(0);
    Lib_WlsInit();

    // 4G APN adaptive registration network
    // 0: Non mandatory automatic adaptation
    Lib_WlsSetAdaptiveAPN(0);
}

/* Status bar / nav bar are both drawn on lv_layer_top(); pages only handle the middle content area */
static void app_start_services(void)
{
    app_network_service();

#if defined(MQTT_SUPPORT)
    Lib_osiThreadCreate("mqttThread", app_mqtt_thread, NULL, OSI_PRIORITY_NORMAL, (unsigned long)32 * 1024, 0);
#endif
}

void AppStart(void)
{
    app_event_init();
    app_ui_init();
    app_page_init();

    /* Power management: brightness output + idle sleep / wake */
    app_power_init();

    /* Top status bar: time / battery / network signal */
    app_status_bar_init();

    /* Bottom nav bar: menu / home / back -> page stack */
    app_nav_bar_init();

    /* Root page: main menu */
    app_page_push(page_home_get());

    app_start_services();

    while (1)
    {
        app_event_t ev;
        bool woken = false;

        /* LVGL main loop (includes lv_tick_inc + lv_timer_handler + input device scan) */
        Lib_LvglTask();

        /* No-operation timeout sleep (default 60s, editable on the settings page; no sleep when external power is connected) */
        //DeviceSleepProcess();

        /* Power key: short press toggles screen, long press opens power menu */
        app_power_key_poll();

        /* Idle timeout auto screen-off + handle touch wake requests */
        app_power_tick();

        /* Tamper check: once per minute, posts APP_EV_TAMPER on a new trigger */
        page_tamper_tick();

        /* Event dispatch */
        while (app_event_fetch(&ev))
        {
            /* While screen is off, any key is only used to wake up and does not enter UI logic;
               tamper alarm events are not dropped and must still open the alarm page after wake */
            if (!app_power_is_screen_on())
            {
                if (!woken)
                {
                    app_power_screen_on();
                    woken = true;
                }

                if (ev.type != APP_EV_TAMPER)
                {
                    continue;
                }
            }

            if (ev.type == APP_EV_KEY)
            {
                app_power_activity();
            }

            app_page_dispatch(&ev);
        }

        Lib_DelayMs(APP_LOOP_INTERVAL_MS);
    }
}
