/**********************************************************************************************************
 * File Name     : app_keyboard.h
 * Description   : Generic input keyboard (mask + title + text box + soft keyboard)
 *
 *   Only one may be open at a time; internally attached to lv_layer_top() and can cover pages and the bottom nav bar.
 *   Character set: ASCII. In password mode the input is displayed as *.
 **********************************************************************************************************/

#ifndef __APP_KEYBOARD_H__
#define __APP_KEYBOARD_H__

#include "lvgl.h"

typedef void (*app_kb_done_cb)(const char *text, void *user_data);
typedef void (*app_kb_cancel_cb)(void *user_data);

/* Open the keyboard; returns false if a keyboard is already open */
bool app_keyboard_open(const char *title,
                       const char *initial_text,
                       bool password,
                       unsigned max_len,
                       app_kb_done_cb on_done,
                       app_kb_cancel_cb on_cancel,
                       void *user_data);

/* Close the keyboard (triggers no callback) */
void app_keyboard_close(void);

bool app_keyboard_is_open(void);

#endif /* __APP_KEYBOARD_H__ */
