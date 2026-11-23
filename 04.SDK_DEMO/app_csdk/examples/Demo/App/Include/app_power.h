/**********************************************************************************************************
 * File Name     : app_power.h
 * Description   : Power key handling and idle sleep / wakeup
 *
 *   Note: this header does not use bool -- LVGL's lvgl.h redefines bool as unsigned char,
 *   which conflicts with bool from <stdbool.h>; unsigned char is used uniformly here to represent on/off state.
 **********************************************************************************************************/

#ifndef __APP_POWER_H__
#define __APP_POWER_H__

/* Init: apply the persisted backlight brightness, reset the idle timer */
void app_power_init(void);

/* Power key polling (called from the main loop): short press toggles the screen, long press shows the power menu */
void app_power_key_poll(void);

/* Idle sleep check and wakeup request handling (called from the main loop) */
void app_power_tick(void);

/* Record one user activity (key / touch): reset the sleep timer; set the wakeup request when the screen is off */
void app_power_activity(void);

/* Temporarily disable / re-enable idle sleep (e.g. during TMS upgrade download), enable: 1 disable, 0 re-enable */
void app_power_prevent_sleep(unsigned char enable);

/* Returns 1 if the screen is currently on, 0 if it is off */
unsigned char app_power_is_screen_on(void);

void app_power_screen_on(void);
void app_power_screen_off(void);
void app_power_toggle_screen(void);

#endif /* __APP_POWER_H__ */
