/**********************************************************************************************************
 * File Name     : app_event.c
 * Description   : Application event queue
 **********************************************************************************************************/

#include "app_event.h"
#include "LibOsi.h"

static void *s_eventQueue = NULL;

void app_event_init(void)
{
    if (s_eventQueue == NULL)
    {
        s_eventQueue = Lib_osiCreatQueue(APP_EVENT_QUEUE_SIZE, sizeof(app_event_t));
    }
}

bool app_event_post(const app_event_t *ev)
{
    if (s_eventQueue == NULL || ev == NULL)
    {
        return false;
    }

    Lib_osiQueueSend(s_eventQueue, (void *)ev, 0);

    return true;
}

bool app_event_post_key(unsigned char key)
{
    app_event_t ev;

    ev.type  = APP_EV_KEY;
    ev.param = (int32_t)key;
    ev.data  = NULL;

    return app_event_post(&ev);
}

bool app_event_post_simple(uint16_t type, int32_t param)
{
    app_event_t ev;

    ev.type  = type;
    ev.param = param;
    ev.data  = NULL;

    return app_event_post(&ev);
}

bool app_event_fetch(app_event_t *ev)
{
    app_event_t tmp;

    if (s_eventQueue == NULL || ev == NULL)
    {
        return false;
    }

    /* Use a sentinel to tell whether an event was really fetched, avoiding reliance on the underlying queue's return value convention */
    tmp.type  = APP_EV_NONE;
    tmp.param = 0;
    tmp.data  = NULL;

    Lib_osiQueueReceive(s_eventQueue, &tmp, 0);

    if (tmp.type == APP_EV_NONE)
    {
        return false;
    }

    *ev = tmp;

    return true;
}
