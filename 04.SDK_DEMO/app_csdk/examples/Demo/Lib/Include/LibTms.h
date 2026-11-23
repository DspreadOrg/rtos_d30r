//-----------------------------------------------------------------------------
// @file    : LibTms.h
// @brief   : TMS remote upgrade public API
// @details : The library talks to the SDK directly (network / file / verify /
//            install / reboot). The APP only receives events and progress.
//-----------------------------------------------------------------------------
#ifndef __LIB_TMS_H__
#define __LIB_TMS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Module to be upgraded. The value is also used as the "param" of the
   TMS_EVT_DOWNLOADING event and as the "module" of TmsProgressFn. */
typedef enum
{
    TMS_MODULE_FIRMWARE = 0,    /* Main application firmware      */
    TMS_MODULE_GPRS,            /* GPRS module firmware           */
    TMS_MODULE_WIFI,            /* WIFI module firmware           */
    TMS_MODULE_SE,              /* Secure element (SE) firmware   */
    TMS_MODULE_FS               /* File system package            */
} TmsModule;

/* Unified return value. */
typedef enum
{
    TMS_OK              = 0,
    TMS_ERR_PARAM       = -1,   /* Invalid argument                        */
    TMS_ERR_BUSY        = -2,   /* An upgrade is already running           */
    TMS_ERR_FILE        = -3,   /* Local file open / read / write failed   */
    TMS_ERR_NET         = -4,   /* Network connect / send / receive failed */
    TMS_ERR_TIMEOUT     = -5,   /* Network timeout                         */
    TMS_ERR_DATA        = -6,   /* Packet / LRC / protocol data error      */
    TMS_ERR_SERVER      = -7,   /* Server rejected the request             */
    TMS_ERR_VERIFY      = -8,   /* Local signature verification failed     */
    TMS_ERR_INSTALL     = -9,   /* Install failed                          */
    TMS_ERR_CANCEL      = -1003 /* Cancelled by Lib_TmsCancel()            */
} TmsResult;

/* Event reported by the library. The APP decides how (and whether) to display it. */
typedef enum
{
    TMS_EVT_CONNECTING = 0,     /* Connecting to the TMS server                 */
    TMS_EVT_TASK_QUERY,         /* Module info reported, querying task          */
    TMS_EVT_NO_TASK,            /* No download task on server                   */
    TMS_EVT_IS_LATEST,          /* Already the latest version                   */
    TMS_EVT_NO_MATCH,           /* No firmware matches this device (PN/type)    */
    TMS_EVT_DOWNLOADING,        /* Entering download phase (see on_progress)    */
    TMS_EVT_DOWNLOAD_DONE,      /* File reception finished                      */
    TMS_EVT_RETRYING,           /* Retrying, param = retry counter (1-based)    */
    TMS_EVT_VERIFYING,          /* Verifying / installing                       */
    TMS_EVT_VERIFY_FAIL,        /* Local signature verification failed          */
    TMS_EVT_INSTALL_FAIL,       /* Installation failed                          */
    TMS_EVT_SUCCESS,            /* Upgrade succeeded                            */
    TMS_EVT_CANCELLED,          /* Cancelled by user                            */
    TMS_EVT_NET_ERROR,          /* Network error                                */
    TMS_EVT_DATA_ERROR,         /* Packet / LRC / data error                    */
    TMS_EVT_SERVER_REPLY        /* Server reply code, param = BCD 0x01..0x10;
                                   the library never renders text, the APP
                                   decides how to display it                */
} TmsEventId;

/* Called in the upgrading thread whenever an event occurs. */
typedef void (*TmsEventFn)(TmsEventId event, int param, void *user);

/* Called in the upgrading thread with throttled progress (about 10Hz).
   "module" is a TmsModule value. */
typedef void (*TmsProgressFn)(TmsModule module, unsigned long done,
                              unsigned long total, void *user);

typedef struct
{
    TmsEventFn    on_event;
    TmsProgressFn on_progress;
    void         *user;
} TmsCallbacks;

//-----------------------------------------------------------------------------
// Name    : Lib_TmsSetCallbacks
// Func    : Register event / progress callbacks. NULL keeps the previous one.
// Ret     : TMS_OK on success, TMS_ERR_PARAM on invalid argument
//-----------------------------------------------------------------------------
int Lib_TmsSetCallbacks(const TmsCallbacks *callbacks);

//-----------------------------------------------------------------------------
// Name    : Lib_TmsParamInit
// Func    : Create the TMS parameter file with default values when missing
// Ret     : TMS_OK on success
//-----------------------------------------------------------------------------
int Lib_TmsParamInit(void);

//-----------------------------------------------------------------------------
// Name    : Lib_TmsUpgrade
// Func    : Run one complete upgrade (query, download, verify, install)
//           The network channel is decided by the SDK, no mode is needed here
// Paras   : module[in],      TMS_MODULE_xxx
//           timeout[in],     network timeout in milliseconds
// Ret     : TMS_OK - success, TMS_ERR_CANCEL - cancelled, other - fail
//-----------------------------------------------------------------------------
TmsResult Lib_TmsUpgrade(TmsModule module, int timeout);

//-----------------------------------------------------------------------------
// Name    : Lib_TmsCancel
// Func    : Request cancel of the upgrade that is currently running. The
//           library returns TMS_ERR_CANCEL at the next packet boundary.
//           Stale requests are discarded automatically when the next
//           upgrade starts, no explicit reset is needed.
//-----------------------------------------------------------------------------
void Lib_TmsCancel(void);

//-----------------------------------------------------------------------------
// Name    : Lib_TmsDeletePackage
// Func    : Delete the downloaded package of one module
//-----------------------------------------------------------------------------
void Lib_TmsDeletePackage(TmsModule module);

//-----------------------------------------------------------------------------
// Name    : Lib_TmsGetVersion
// Func    : Get the TMS library version (static string, do not free)
//-----------------------------------------------------------------------------
const char *Lib_TmsGetVersion(void);

#ifdef __cplusplus
}
#endif

#endif
