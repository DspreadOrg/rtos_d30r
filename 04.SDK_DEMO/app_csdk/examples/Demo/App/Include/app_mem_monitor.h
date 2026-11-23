/**********************************************************************************************************
 * File Name     : app_mem_monitor.h
 * Description   : Memory status floating panel (attached to lv_layer_top, persists across page switches)
 **********************************************************************************************************/

#ifndef __APP_MEM_MONITOR_H__
#define __APP_MEM_MONITOR_H__

#include "lvgl.h"

/* Show / hide the floating panel */
void app_mem_monitor_toggle(void);
bool app_mem_monitor_is_open(void);

#endif /* __APP_MEM_MONITOR_H__ */
