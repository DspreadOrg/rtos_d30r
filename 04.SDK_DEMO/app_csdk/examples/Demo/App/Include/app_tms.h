/**********************************************************************************************************
 * File Name     : app_tms.h
 * Description   : TMS remote update entry
 *
 *   All communication, parsing, verification and installation are done inside libTms.a. This layer only
 *   calls Lib_TmsUpgrade() and forwards the events to the page, which decides how to display progress,
 *   errors and the server reply text.
 **********************************************************************************************************/

#ifndef __APP_TMS_H__
#define __APP_TMS_H__

#include "LibTms.h"

/* ---------------- Download entry ---------------- */

/* Trigger a TMS remote update for one module.
   Blocking: performs connect, query, download, verify and install internally, so it must be called from
   a worker thread (the UI thread must stay responsive).
   module  : TMS_MODULE_xxx
   timeout : network timeout in milliseconds
   return  : TMS_OK on success, TMS_ERR_CANCEL when cancelled, other TmsResult on failure */
TmsResult app_tms_download(TmsModule module, int timeout);

/* ---------------- Callbacks ---------------- */

/* Flow event callback (runs in the download thread; do not touch LVGL widgets here). */
typedef void (*app_tms_event_cb)(TmsEventId event, int param);

/* Progress callback: done/total are the downloaded/total bytes, module is the TmsModule.
   libTms already rate-limits to 10Hz internally; the first and last packets are always sent. */
typedef void (*app_tms_progress_cb)(TmsModule module, unsigned long done, unsigned long total);

/* Register / unregister the event and progress callbacks. page_tms registers them in on_create and
   unregisters in on_destroy. Passing NULL means that type of callback is not of interest. */
void app_tms_set_callbacks(app_tms_event_cb on_event, app_tms_progress_cb on_progress);

/* ---------------- Cancel control ---------------- */

/* Request cancellation of the ongoing TMS download (nav-bar back key / page_tms_cancel).
   libTms detects it at the next packet boundary and returns TMS_ERR_CANCEL. A stale
   request is discarded automatically when the next download starts. */
void app_tms_request_cancel(void);

#endif /* __APP_TMS_H__ */
