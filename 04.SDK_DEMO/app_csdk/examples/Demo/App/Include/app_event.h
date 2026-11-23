/**********************************************************************************************************
 * File Name     : app_event.h
 * Description   : Application event model (key / network / battery / TMS / print / scan / power / tamper)
 **********************************************************************************************************/

#ifndef __APP_EVENT_H__
#define __APP_EVENT_H__

#include <stdbool.h>
#include <stdint.h>

#define APP_EVENT_QUEUE_SIZE    16

typedef enum
{
    APP_EV_NONE = 0,
    APP_EV_KEY,             /* param = KB_* in Key.h */
    APP_EV_NET_STATUS,      /* param = 0 disconnected / 1 connected */
    APP_EV_BATTERY,         /* param = battery percentage */
    APP_EV_TIME_TICK,       /* param = seconds */
    APP_EV_TMS_EVENT,       /* TMS flow event: param low 16 bits = TmsEventId, high 16 bits = extra parameter */
    APP_EV_TMS_PROGRESS,    /* TMS download progress: param = percentage 0~100 (posted by download thread) */
    APP_EV_TMS_DONE,        /* TMS download finished: param = app_tms_download return code (posted by download thread) */
    APP_EV_MAINT_START,     /* Request to enter maintenance mode (param unused) */
    APP_EV_PRINT_DONE,      /* param = 0 failed / 1 succeeded */
    APP_EV_SCAN_RESULT,     /* data  = barcode string */
    APP_EV_IDLE_TIMEOUT,
    APP_EV_POWER,           /* param = power action */
    APP_EV_TAMPER,          /* param = tamper code (Lib_TamperIsOccured output) */
} app_event_type_t;

typedef struct
{
    uint16_t type;
    int32_t  param;
    void    *data;
} app_event_t;

void app_event_init(void);

bool app_event_post(const app_event_t *ev);
bool app_event_post_key(unsigned char key);
bool app_event_post_simple(uint16_t type, int32_t param);

/* Non-blocking event fetch; returns true if ev is valid */
bool app_event_fetch(app_event_t *ev);

#endif /* __APP_EVENT_H__ */
