/**********************************************************************************************************
 * File Name     : app_power.c
 * Description   : Power key and idle sleep/wakeup (480x854 handheld terminal)
 *
 *   The power key works like a mobile phone:
 *     short press (released in < 1.5s) -> toggle screen off / on
 *     long press (>= 1.5s)             -> show power menu (shutdown / reboot / cancel)
 *   Idle sleep:
 *     if there is no key or touch activity within E_PARAM_SLEEP_TIME seconds, the screen turns off automatically; 0 means never sleep
 *   Wakeup:
 *     the power key, any other key, or a touch tap can wake the device; the wakeup action itself does not trigger any UI operation
 **********************************************************************************************************/

#include "app_power.h"
#include "app_event.h"
#include "app_ui.h"
#include "ui_status_bar.h"
#include "LibLcd.h"
#include "LibLvgl.h"
#include "LibParamManage.h"
#include "LibSysapi.h"

#define POWER_LONG_PRESS_MS     2000    /* long press threshold, the power menu is shown once exceeded */
#define POWER_MIN_PRESS_MS      50      /* minimum press time for a short press, filters key bounce */
#define POWER_PRESS_STALE_MS    (POWER_LONG_PRESS_MS + 500)   /* press state stale threshold */
#define POWER_BRIGHT_MIN        1       /* consistent with the 0~8 levels of Lib_LcdSetBrightLevel */
#define POWER_BRIGHT_MAX        7       /* matches the OS maximum brightness */
#define POWER_BRIGHT_DEFAULT    5
#define POWER_SLEEP_DEFAULT_SEC 60      /* sleep duration used when the parameter cannot be read */
#define POWER_SLEEP_CHECK_MS    1000    /* sleep timeout check period */

static unsigned char s_screenOn = 1;        /* whether the screen is currently on */
static unsigned char s_wakeRequest = 0;     /* wakeup request from non-main-loop contexts such as touch */
static unsigned char s_sleepHold = 0;       /* 1 means auto sleep is temporarily disabled (e.g. during TMS download) */
static unsigned long s_lastActive = 0;      /* time of the most recent user activity (ms) */
static unsigned long s_lastCheck = 0;       /* time of the most recent sleep check (ms) */
static unsigned long s_pressTick = 0;       /* start time of the power key press (ms) */
static unsigned char s_pressed = 0;         /* whether the power key is currently pressed (sentinel) */
static unsigned char s_longDone = 0;        /* long press already triggered for this press */
static unsigned char s_wokeByPress = 0;     /* this press was "power key wakeup while the screen is off" */

/* Read the persisted brightness; fall back to the middle level on invalid values */
static unsigned char power_bright_get(void)
{
    unsigned char level = POWER_BRIGHT_DEFAULT;

    if (Lib_ParamGetU8(E_PARAM_BRIGHT_LEVEL, &level) != RET_OK)
    {
        return POWER_BRIGHT_DEFAULT;
    }

    if ((level < POWER_BRIGHT_MIN) || (level > POWER_BRIGHT_MAX))
    {
        return POWER_BRIGHT_DEFAULT;
    }

    return level;
}

void app_power_init(void)
{
    s_screenOn = 1;
    s_wakeRequest = 0;
    s_lastActive = Lib_SysGetTimer();
    s_lastCheck = s_lastActive;
    Lib_Trace("[PWR] init t=%lu\n", s_lastActive);
}

unsigned char app_power_is_screen_on(void)
{
    return s_screenOn;
}

void app_power_screen_off(void)
{
    if (!s_screenOn)
    {
        return;
    }

    s_screenOn = 0;
    Lib_Trace("[PWR] screen_off enter t=%lu\n", Lib_SysGetTimer());

    Lib_LvglSleep();
    /* Blocking: returns only after a wakeup source (power key / external power / slave key, etc.) triggers */
    Lib_SysSleep();
    Lib_Trace("[PWR] syssleep ret t=%lu\n", Lib_SysGetTimer());

    /* Returning means the device has been woken up: the state must be synced back to screen-on and the idle timer reset,
       otherwise s_screenOn stays at 0, the idle sleep check in app_power_tick() always returns early,
       and the screen never turns off automatically after the first screen-off */
    app_power_screen_on();

    /* This wakeup key press has already been consumed by the sleep flow: releasing it must not be treated as a short-press screen-off,
       and holding it must not be treated as a long press opening the power menu (users often keep pressing a while after screen-on) */
    s_pressed = 1;
    s_pressTick = Lib_SysGetTimer();
    s_longDone = 1;
    s_wokeByPress = 1;
    s_screenOn = 1;
    Lib_Trace("[PWR] wake consumed t=%lu\n", s_pressTick);
}

void app_power_screen_on(void)
{
    Lib_Trace("[PWR] screen_on enter t=%lu on=%d\n", Lib_SysGetTimer(), s_screenOn);

    if (s_screenOn)
    {
        app_power_activity();
        return;
    }

    s_screenOn = 1;

    Lib_LvglWakeUp();
    Lib_LcdSetBrightLevel(power_bright_get());

    /* The screen was cleared and LVGL rendering was paused during screen-off, so redraw the whole screen once after wakeup */
    lv_obj_invalidate(lv_scr_act());
    lv_obj_invalidate(lv_layer_top());

    app_status_bar_refresh_signal();

    s_lastActive = Lib_SysGetTimer();
    s_lastCheck = s_lastActive;
}

void app_power_toggle_screen(void)
{
    if (s_screenOn)
    {
        app_power_screen_off();
    }
    else
    {
        app_power_screen_on();
    }
}

void app_power_activity(void)
{
    s_lastActive = Lib_SysGetTimer();

    if (!s_screenOn)
    {
        /* May be called from an LVGL input callback; only set the flag, the actual wakeup is left to the main loop */
        s_wakeRequest = 1;
    }
}

void app_power_key_poll(void)
{
    unsigned long now = Lib_SysGetTimer();

    if (Lib_GetPwrKeyStatus())
    {
        /* A new press: also a fallback for the previous release not being polled */
        if (!s_pressed || (now < s_pressTick)
            || ((now - s_pressTick) > POWER_PRESS_STALE_MS))
        {
            Lib_Trace("[PWR] key down t=%lu sp=%d on=%d lastTick=%lu\n",
                      now, s_pressed, s_screenOn, s_pressTick);

            s_pressed = 1;
            s_pressTick = now;
            s_longDone = 0;
            s_wokeByPress = 0;

            if (!s_screenOn)
            {
                app_power_screen_on();
                s_wokeByPress = 1;
            }
            else
            {
                app_power_activity();
            }
        }
        else if (!s_longDone && ((now - s_pressTick) >= POWER_LONG_PRESS_MS))
        {
            s_longDone = 1;
            app_power_screen_on();
            app_event_post_key(KB_POWER_OFF);
            Lib_Trace("[PWR] LONG PRESS t=%lu dt=%lu -> push power page\n",
                      now, now - s_pressTick);
        }
    }
    else
    {
        unsigned char shortPress = 0;
        unsigned long dt = (now >= s_pressTick) ? (now - s_pressTick) : 0;

        if (s_pressed && !s_longDone && !s_wokeByPress
            && (now >= s_pressTick)
            && (dt >= POWER_MIN_PRESS_MS))
        {
            shortPress = 1;
        }

        if (s_pressed)
        {
            Lib_Trace("[PWR] key up t=%lu dt=%lu long=%d woke=%d short=%d\n",
                      now, dt, s_longDone, s_wokeByPress, shortPress);
        }

        /* Clear the key state first: a short press enters sleep and blocks until the next wakeup,
           so the "this press consumed" flag written by the wakeup flow must not be overwritten here */
        s_pressed = 0;
        s_longDone = 0;
        s_wokeByPress = 0;

        if (shortPress)
        {
            app_power_toggle_screen();
        }
    }
}

void app_power_prevent_sleep(unsigned char enable)
{
    s_sleepHold = enable ? 1 : 0;

    /* Restart the timer when sleep is re-enabled, avoiding an immediate timeout screen-off right after resuming */
    s_lastActive = Lib_SysGetTimer();
}

void app_power_tick(void)
{
    unsigned long now = Lib_SysGetTimer();
    unsigned int sleepSec = POWER_SLEEP_DEFAULT_SEC;

    /* Wakeup requests set by input callbacks such as touch are actually performed here */
    if (s_wakeRequest)
    {
        s_wakeRequest = 0;
        app_power_screen_on();

        /* screen_on writes a timestamp newer than now; continuing the calculation would underflow unsigned values,
           so return first and recount the idle time on the next tick */
        return;
    }

    if (!s_screenOn)
    {
        return;
    }

    if (s_sleepHold)
    {
        s_lastActive = now;
        return;
    }

    if ((now - s_lastCheck) < POWER_SLEEP_CHECK_MS)
    {
        return;
    }

    s_lastCheck = now;

    /* The sleep duration is read each time so changes in the settings page take effect immediately; 0 means never sleep */
    if (Lib_ParamGetU32(E_PARAM_SLEEP_TIME, &sleepSec) != RET_OK)
    {
        sleepSec = POWER_SLEEP_DEFAULT_SEC;
    }
    Lib_Trace("sleepSec: %d", sleepSec);

    if (sleepSec == 0)
    {
        s_lastActive = now;
        return;
    }

    if ((now - s_lastActive) >= ((unsigned long)sleepSec * 1000UL))
    {
        Lib_Trace("[PWR] idle timeout t=%lu idle=%lu sleepSec=%u\n",
                  now, now - s_lastActive, sleepSec);
        app_power_screen_off();
    }
}
