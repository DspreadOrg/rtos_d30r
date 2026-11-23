/**********************************************************************************************************
 * File Name     : app_tms.c
 * Description   : TMS remote update entry
 *
 *   Only does "initialize parameters + call Lib_TmsUpgrade" and forwards the
 *   library callbacks to the page. No protocol / parsing logic lives here any more.
 **********************************************************************************************************/

#include "app_tms.h"
#include "app_power.h"

#include <stddef.h>

static app_tms_event_cb    s_onEvent = NULL;
static app_tms_progress_cb s_onProgress = NULL;

static void app_tms_on_event(TmsEventId event, int param, void *user)
{
    (void)user;

    if (s_onEvent != NULL)
    {
        s_onEvent(event, param);
    }
}

static void app_tms_on_progress(TmsModule module, unsigned long done,
                                unsigned long total, void *user)
{
    (void)user;

    if (s_onProgress != NULL)
    {
        s_onProgress(module, done, total);
    }
}

void app_tms_set_callbacks(app_tms_event_cb on_event, app_tms_progress_cb on_progress)
{
    TmsCallbacks callbacks;

    s_onEvent = on_event;
    s_onProgress = on_progress;

    callbacks.on_event = app_tms_on_event;
    callbacks.on_progress = app_tms_on_progress;
    callbacks.user = NULL;
    Lib_TmsSetCallbacks(&callbacks);
}

void app_tms_request_cancel(void)
{
    Lib_TmsCancel();
}

TmsResult app_tms_download(TmsModule module, int timeout)
{
    TmsResult ret;

    app_power_prevent_sleep(1);

    Lib_TmsParamInit();
    ret = Lib_TmsUpgrade(module, timeout);

    app_power_prevent_sleep(0);

    return ret;
}
