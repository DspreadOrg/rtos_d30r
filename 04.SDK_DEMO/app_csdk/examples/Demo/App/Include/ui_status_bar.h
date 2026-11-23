/**********************************************************************************************************
 * File Name     : ui_status_bar.h
 * Description   : D30R top status bar (time / battery / network signal)
 **********************************************************************************************************/

#ifndef __UI_STATUS_BAR_H__
#define __UI_STATUS_BAR_H__

/* Create the 480 x 30 status bar on lv_layer_top() and start the 1-second refresh timer */
void app_status_bar_init(void);

/* Immediately refresh the status bar signal icon and network type text for the current network mode (call after switching networks) */
void app_status_bar_refresh_signal(void);

/* Re-apply the status bar fonts for the current language (call after switching language) */
void app_status_bar_refresh_font(void);

#endif /* __UI_STATUS_BAR_H__ */
