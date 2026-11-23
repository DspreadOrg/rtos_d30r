/**********************************************************************************************************
 * File Name     : ui_status_bar.c
 * Description   : D30R top status bar (480 x 30)
 *
 *   The old implementation (LvUiCreateStatusBar / LvUiStatusBarTask in Driver/Source/api_lvgl.c) displayed
 *   time (centered), battery and WiFi / GPRS signal, but its icons relied on the bitmap resources in
 *   Resource.h, which were removed with the Driver directory. This redraws it with LVGL widgets, in app_ui.h colors.
 *
 *   The debug / speaker icons in the old status_bar_cb() are no longer drawn (image resources gone).
 *
 *   Signal icon: vertical bars for 4G mode, Android-style fan (lv_arc concentric arcs, opening up) for WiFi.
 **********************************************************************************************************/

#include "ui_status_bar.h"
#include "app_ui.h"
#include "LibSysapi.h"      /* Lib_GetDateTime / Lib_ExtPower */
#include "LibBattery.h"     /* Lib_BatGetCapacity / Lib_BatGetChargeState */
#include "LibWifi.h"        /* Lib_WifiGetApStatus / Lib_WifiCheckSignal */
#include "LibGprs.h"        /* Lib_WlsGetInitStatus / Lib_WlsCheckSignal / Lib_WlsGetNetMode */
#include "LibNetwork.h"     /* NET_MODE_GPRS */
#include "LibParamManage.h" /* Lib_ParamGetU8 / E_PARAM_NETWORK_MODE */

#include <stdio.h>
#include <string.h>

#define SB_REFRESH_MS       1000    /* time refresh period */
#define SB_NET_TICKS        5       /* signal / battery refreshed every 5 s */
#define SB_SIG_BAR_COUNT    4
#define SB_SIG_BAR_W        4
#define SB_SIG_BAR_GAP      2
#define SB_SIG_BASE_Y       (APP_STATUS_BAR_H - 7)   /* bar bottom edge: tallest bar y8~23, vertically centered with battery (y9~22) */
#define SB_SIG_X            12

/* WiFi fan (Android style): center at the bottom, opening upward.
   Visible area is roughly CY-15 ~ CY-1 (above the center); CY is APP_STATUS_BAR_H-6
   so the fan is vertically centered with the battery icon (y 9~22, center 15.5) */
#define SB_WIFI_ICON_SIZE   28
#define SB_WIFI_CX          22
#define SB_WIFI_CY          (APP_STATUS_BAR_H - 6)

#define SB_BAT_W            26
#define SB_BAT_H            13
#define SB_BAT_INNER_W      20
#define SB_BAT_INNER_H      7
#define SB_BAT_HEAD_W       3       /* battery positive cap width */
#define SB_BAT_HEAD_H       6       /* battery positive cap height */
#define SB_BAT_HEAD_OFF     (-7)    /* cap right offset from the status bar right edge (battery shell right edge is -10) */
#define SB_BAT_HEAD_Y       12      /* cap vertical position (battery shell y=9, height 13, cap centered) */
#define SB_BAT_ANIM_MS      3000    /* duration of one "empty -> full" charging animation cycle */

static lv_obj_t *s_bar = NULL;
static lv_obj_t *s_timeLabel = NULL;
static lv_obj_t *s_netLabel = NULL;
static lv_obj_t *s_batLabel = NULL;
static lv_obj_t *s_batFill = NULL;
static lv_obj_t *s_batHead = NULL;
static unsigned char s_batAnimating = 0;    /* whether the charging animation is currently running */
static unsigned char s_batCap = 0xFF;       /* last rendered battery percentage (0xFF = uninitialized) */
static unsigned char s_batCharging = 0;     /* last rendered charging state */
static lv_obj_t *s_sigBar[SB_SIG_BAR_COUNT] = {0};
static lv_obj_t *s_wifiIcon = NULL;
static unsigned char s_refreshTick = 0;
static unsigned char s_sigLevel = 0xFF;     /* last rendered signal level (0xFF = uninitialized) */
static signed char s_sigModeGprs = -1;      /* last rendered network mode (-1 = uninitialized) */
static char s_netText[8] = "";      /* cached network type text, avoids redraw when unchanged */

/* ---------------- Create ---------------- */

static void status_bar_create_signal(lv_obj_t *bar)
{
    static const lv_coord_t barHeight[SB_SIG_BAR_COUNT] = {6, 9, 12, 15};
    int i = 0;

    for (i = 0; i < SB_SIG_BAR_COUNT; i++)
    {
        lv_obj_t *sig = lv_obj_create(bar);

        if (sig == NULL)
        {
            continue;
        }

        lv_obj_remove_style_all(sig);
        lv_obj_set_size(sig, SB_SIG_BAR_W, barHeight[i]);
        lv_obj_set_pos(sig, SB_SIG_X + i * (SB_SIG_BAR_W + SB_SIG_BAR_GAP),
                       SB_SIG_BASE_Y - barHeight[i]);
        lv_obj_set_style_radius(sig, 1, 0);
        lv_obj_set_style_bg_color(sig, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(sig, LV_OPA_30, 0);
        lv_obj_clear_flag(sig, LV_OBJ_FLAG_SCROLLABLE);

        s_sigBar[i] = sig;
    }

    /* Android-style fan icon for WiFi mode, toggled against the 4G bars */
    s_wifiIcon = app_wifi_signal_icon(bar, SB_WIFI_ICON_SIZE, lv_color_white());
    if (s_wifiIcon != NULL)
    {
        lv_obj_set_pos(s_wifiIcon,
                       SB_WIFI_CX - SB_WIFI_ICON_SIZE / 2,
                       SB_WIFI_CY - SB_WIFI_ICON_SIZE);
    }

    /* GPRS mode shows 2G / 3G / 4G, WiFi mode shows WiFi */
    s_netLabel = lv_label_create(bar);
    if (s_netLabel != NULL)
    {
        lv_label_set_text(s_netLabel, "");
        lv_obj_set_style_text_color(s_netLabel, lv_color_white(), 0);
        lv_obj_set_style_text_font(s_netLabel, app_font(14), 0);
        lv_obj_set_pos(s_netLabel,
                       SB_SIG_X + SB_SIG_BAR_COUNT * (SB_SIG_BAR_W + SB_SIG_BAR_GAP) + 4, 8);
    }
}

static void status_bar_create_battery(lv_obj_t *bar)
{
    lv_obj_t *outline = lv_obj_create(bar);
#if 0
    s_batLabel = lv_label_create(bar);
    if (s_batLabel != NULL)
    {
        lv_label_set_text(s_batLabel, "--%");
        lv_obj_set_style_text_color(s_batLabel, lv_color_white(), 0);
        lv_obj_set_style_text_font(s_batLabel, app_font(16), 0);
        lv_obj_align(s_batLabel, LV_ALIGN_TOP_RIGHT, -44, 6);
    }
#endif
    if (outline == NULL)
    {
        return;
    }

    lv_obj_remove_style_all(outline);
    lv_obj_set_size(outline, SB_BAT_W, SB_BAT_H);
    lv_obj_align(outline, LV_ALIGN_TOP_RIGHT, -10, 9);
    lv_obj_set_style_radius(outline, 3, 0);
    lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_border_color(outline, lv_color_white(), 0);
    lv_obj_set_style_border_opa(outline, LV_OPA_80, 0);
    lv_obj_clear_flag(outline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(outline, LV_OBJ_FLAG_CLICKABLE);

    /* battery fill (width varies with percentage) */
    s_batFill = lv_obj_create(outline);
    if (s_batFill != NULL)
    {
        lv_obj_remove_style_all(s_batFill);
        lv_obj_set_pos(s_batFill, 1, 1);
        lv_obj_set_size(s_batFill, 0, SB_BAT_INNER_H);
        lv_obj_set_style_radius(s_batFill, 1, 0);
        lv_obj_set_style_bg_color(s_batFill, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(s_batFill, LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_batFill, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* battery positive cap attached to the shell's right side, to look like a real battery */
    s_batHead = lv_obj_create(bar);
    if (s_batHead != NULL)
    {
        lv_obj_remove_style_all(s_batHead);
        lv_obj_set_size(s_batHead, SB_BAT_HEAD_W, SB_BAT_HEAD_H);
        lv_obj_align(s_batHead, LV_ALIGN_TOP_RIGHT, SB_BAT_HEAD_OFF, SB_BAT_HEAD_Y);
        lv_obj_set_style_radius(s_batHead, 1, 0);
        lv_obj_set_style_bg_color(s_batHead, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(s_batHead, LV_OPA_80, 0);
        lv_obj_clear_flag(s_batHead, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_batHead, LV_OBJ_FLAG_CLICKABLE);
    }
}

/* convert percentage to battery fill width */
static lv_coord_t status_bar_bat_fill_width(unsigned char cap)
{
    lv_coord_t width = (lv_coord_t)((cap * SB_BAT_INNER_W) / 100);

    return (width < 1) ? 0 : width;
}

/* charging animation: fill width loops between empty and full, only indicating "charging" */
static void status_bar_bat_anim_cb(void *var, int32_t value)
{
    lv_obj_set_width((lv_obj_t *)var, (lv_coord_t)value);
}

static void status_bar_bat_anim_start(void)
{
    lv_anim_t anim;

    if (s_batFill == NULL)
    {
        return;
    }

    lv_anim_del(s_batFill, status_bar_bat_anim_cb);

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_batFill);
    lv_anim_set_values(&anim, 2, SB_BAT_INNER_W);
    lv_anim_set_time(&anim, SB_BAT_ANIM_MS);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, status_bar_bat_anim_cb);
    lv_anim_start(&anim);
}

static void status_bar_bat_anim_stop(void)
{
    if (s_batFill == NULL)
    {
        return;
    }

    lv_anim_del(s_batFill, status_bar_bat_anim_cb);

    /* after the animation stops, restore the width for the real capacity */
    lv_obj_set_size(s_batFill, status_bar_bat_fill_width(s_batCap), SB_BAT_INNER_H);
}

/* ---------------- Refresh ---------------- */

static void status_bar_update_time(void)
{
    unsigned char aucTime[6] = {0};
    char acText[8] = {0};

    if (s_timeLabel == NULL)
    {
        return;
    }

    /* Lib_GetDateTime outputs YYMMDDhhmmss (BCD); take hour and minute from bytes 3 and 4 */
    if (Lib_GetDateTime(aucTime) != 0)
    {
        return;
    }

    snprintf(acText, sizeof(acText), "%02X:%02X", aucTime[3], aucTime[4]);
    lv_label_set_text(s_timeLabel, acText);
}

/* whether currently in 4G mode (reads the parameter cache, falls back to WiFi on failure) */
static int status_bar_mode_is_gprs(void)
{
    unsigned char mode = NET_MODE_WIFI;

    if (Lib_ParamGetU8(E_PARAM_NETWORK_MODE, &mode) != RET_OK)
    {
        mode = NET_MODE_WIFI;
    }

    return (mode == NET_MODE_GPRS);
}

/* network type text in 4G mode (2G / 3G / 4G / 5G), shows 4G when unavailable */
static const char *status_bar_gprs_name(void)
{
    int sys = Lib_WlsGetNetMode();

    switch (sys)
    {
    case NET_SYS_2G:
        return "2G";

    case NET_SYS_3G:
        return "3G";

    case NET_SYS_4G:
        return "4G";

    case NET_SYS_5G:
        return "5G";

    default:
        return "4G";
    }
}

/* 4G state: returns signal level 0~4 and outputs a status hint via pText
   (no SIM -> NO SIM, SIM not registered -> No Svc, normal -> 2G/3G/4G/5G) */
static unsigned char status_bar_gprs_state(const char **pText)
{
    int initStatus = Lib_WlsGetInitStatus();
    int raw = 0;

    if (initStatus == WLS_NOSIM)
    {
        *pText = "NO SIM";
        return 0;
    }

    /* any state other than WLS_OK / WLS_ATTACHED is treated as not yet registered */
    if ((initStatus != WLS_OK) && (initStatus != WLS_ATTACHED))
    {
        *pText = "No Svc";
        return 0;
    }

    if (Lib_WlsCheckSignal(&raw) != RET_OK)
    {
        *pText = "No Svc";
        return 0;
    }

    *pText = status_bar_gprs_name();
    return app_signal_level_from_raw(raw);
}

/* WiFi state: shows OFF when not connected, WiFi when connected */
static unsigned char status_bar_wifi_state(const char **pText)
{
    int raw = 0;

    if (Lib_WifiGetApStatus() != STA_AP_CONNECTED)
    {
        *pText = "OFF";
        return 0;
    }

    *pText = "WiFi";

    if (Lib_WifiCheckSignal(&raw) != RET_OK)
    {
        return 0;
    }

    return app_signal_level_from_raw(raw);
}

static void status_bar_update_signal(void)
{
    int isGprs = status_bar_mode_is_gprs();
    const char *net = NULL;
    unsigned char level = 0;
    int i = 0;

    level = isGprs ? status_bar_gprs_state(&net) : status_bar_wifi_state(&net);

    /* switch icon shape on mode change: 4G bars / WiFi fan */
    if (s_sigModeGprs != (signed char)isGprs)
    {
        s_sigModeGprs = (signed char)isGprs;
        s_sigLevel = 0xFF;      /* lit states of the two icon sets are not reusable, force redraw */

        for (i = 0; i < SB_SIG_BAR_COUNT; i++)
        {
            if (s_sigBar[i] != NULL)
            {
                if (isGprs)
                {
                    lv_obj_clear_flag(s_sigBar[i], LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_add_flag(s_sigBar[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        if (s_wifiIcon != NULL)
        {
            if (isGprs)
            {
                lv_obj_add_flag(s_wifiIcon, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_clear_flag(s_wifiIcon, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    if (s_sigLevel != level)
    {
        s_sigLevel = level;

        if (isGprs)
        {
            for (i = 0; i < SB_SIG_BAR_COUNT; i++)
            {
                if (s_sigBar[i] != NULL)
                {
                    lv_obj_set_style_bg_opa(s_sigBar[i],
                                            (i < level) ? LV_OPA_COVER : LV_OPA_30, 0);
                }
            }
        }
        else
        {
            app_wifi_signal_set_level(s_wifiIcon, level);
        }
    }

    if (s_netLabel == NULL)
    {
        return;
    }

    if (strcmp(s_netText, LIB_TR(net)) != 0)
    {
        const char *shown = LIB_TR(net);

        snprintf(s_netText, sizeof(s_netText), "%s", shown);
        lv_label_set_text(s_netLabel, s_netText);
    }
}

static void status_bar_update_battery(void)
{
    unsigned char ucCapacity = Lib_BatGetCapacity();
    unsigned char ucCharging = (Lib_ExtPower() && (Lib_BatGetChargeState() != 0)) ? 1 : 0;
    lv_color_t color = lv_color_hex(0x26D07C);
    unsigned char animate = 0;
    char acText[8] = {0};

    if (ucCapacity > 100)
    {
        ucCapacity = 100;
    }

    /* battery readings are unreliable while charging on this platform, so the animation is driven only by
       "charging or not"; once full, Lib_BatGetChargeState() returns to 0 and the animation stops */
    animate = ucCharging;

    /* skip widgets when capacity and charging state are unchanged, to avoid redrawing the whole bar */
    if ((ucCapacity != s_batCap) || (ucCharging != s_batCharging))
    {
        s_batCap = ucCapacity;
        s_batCharging = ucCharging;

        if (ucCharging == 0)
        {
            if (ucCapacity <= 20)
            {
                color = lv_color_hex(0xE53935);
            }
            else if (ucCapacity <= 60)
            {
                color = lv_color_hex(0xFFB300);
            }
        }

        if (s_batLabel != NULL)
        {
            snprintf(acText, sizeof(acText), "%u%%", (unsigned int)ucCapacity);
            lv_label_set_text(s_batLabel, acText);
        }

        if (s_batFill != NULL)
        {
            lv_obj_set_style_bg_color(s_batFill, color, 0);

            /* while the animation runs it owns the width, to avoid both sides changing it at once */
            if (s_batAnimating == 0)
            {
                lv_obj_set_size(s_batFill,
                                status_bar_bat_fill_width(ucCapacity),
                                SB_BAT_INNER_H);
            }
        }
    }

    /* charging -> fill loops; stops when unplugged or full */
    if (animate)
    {
        if (s_batAnimating == 0)
        {
            s_batAnimating = 1;
            status_bar_bat_anim_start();
        }
    }
    else if (s_batAnimating != 0)
    {
        s_batAnimating = 0;
        status_bar_bat_anim_stop();
    }
}

static void status_bar_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    status_bar_update_time();

    /* check battery / charging state every second for timely animation start/stop (change detection inside) */
    status_bar_update_battery();

    if (++s_refreshTick >= SB_NET_TICKS)
    {
        s_refreshTick = 0;
        status_bar_update_signal();
    }
}

/* ---------------- Entry ---------------- */

void app_status_bar_init(void)
{
    if (s_bar != NULL)
    {
        return;
    }

    s_bar = lv_obj_create(lv_layer_top());
    if (s_bar == NULL)
    {
        return;
    }

    lv_obj_remove_style_all(s_bar);
    lv_obj_set_size(s_bar, LV_PCT(100), APP_STATUS_BAR_H);
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_bar, APP_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_CLICKABLE);

    s_timeLabel = lv_label_create(s_bar);
    if (s_timeLabel != NULL)
    {
        lv_label_set_text(s_timeLabel, "--:--");
        lv_obj_set_style_text_color(s_timeLabel, lv_color_white(), 0);
        lv_obj_set_style_text_font(s_timeLabel, app_font(18), 0);
        lv_obj_align(s_timeLabel, LV_ALIGN_TOP_MID, 0, 4);
    }

    status_bar_create_signal(s_bar);
    status_bar_create_battery(s_bar);

    status_bar_update_time();
    status_bar_update_signal();
    status_bar_update_battery();

    lv_timer_create(status_bar_timer_cb, SB_REFRESH_MS, NULL);
}

void app_status_bar_refresh_font(void)
{
    if (s_netLabel != NULL)
    {
        lv_obj_set_style_text_font(s_netLabel, app_font(14), 0);
    }

    if (s_batLabel != NULL)
    {
        lv_obj_set_style_text_font(s_batLabel, app_font(16), 0);
    }

    if (s_timeLabel != NULL)
    {
        lv_obj_set_style_text_font(s_timeLabel, app_font(18), 0);
    }
}

void app_status_bar_refresh_signal(void)
{
    if (s_bar == NULL)
    {
        return;
    }

    /* reset the tick counter so the next periodic refresh restarts from the switch moment */
    s_refreshTick = 0;
    status_bar_update_signal();
}
