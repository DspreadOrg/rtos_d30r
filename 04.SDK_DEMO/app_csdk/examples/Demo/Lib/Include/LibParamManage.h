//-----------------------------------------------------------------------------
// @file    : LibParamManage.h
// @brief   : Terminal parameter management
// @details : All the terminal parameters are kept in one static RAM cache and
//            persisted in one file (/sdkparam.dat). The file header carries
//            the magic, the version and a CRC16, the payload is a TLV list.
//            Parameters are accessed by ID, the cache and the file are
//            protected by one mutex, so the interfaces may be called from any
//            thread (not from an interrupt).
// @author  :
// @version :
// @date    :
// @history :
//-----------------------------------------------------------------------------
#ifndef _LIB_PARAM_MANAGE_H_
#define _LIB_PARAM_MANAGE_H_

#include "LibTypedef.h"

//-----------------------------------------------------------------------------
// Macro definition
//-----------------------------------------------------------------------------

// Fixed length of each parameter type, in bytes
#define PARAM_RAW_SIZE              32      // raw bytes, not NUL terminated
#define PARAM_U8_SIZE               1
#define PARAM_U32_SIZE              4
#define PARAM_F32_SIZE              4
#define PARAM_STR_SIZE              32      // string slot, ending 0 included

// Maximum length of one parameter, limited by the unsigned char length
#define PARAM_ITEM_MAX_SIZE         255

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

// Global parameter IDs. They are stored in the file as 16-bit values, the
// published values must never be reused. 0x2000 and above are reserved.
typedef enum
{
    E_PARAM_TERMINAL_NO       = 0x1000,   // raw 32 bytes, not NUL terminated
    E_PARAM_NETWORK_MODE      = 0x1001,   // U8
    E_PARAM_BRIGHT_LEVEL      = 0x1002,   // U8
    E_PARAM_BEEP              = 0x1003,   // U8
    E_PARAM_WIFI_SSID         = 0x1004,   // string
    E_PARAM_WIFI_PASS         = 0x1005,   // string
    E_PARAM_TIME_ZONE         = 0x1006,   // U32
    E_PARAM_NTP_SERVER        = 0x1007,   // string
    E_PARAM_NTP_SYNC_SWITCH   = 0x1008,   // U8
    E_PARAM_NTP_SYNC_INTER    = 0x1009,   // U8, unit: hour
    E_PARAM_MANAGE_PWD        = 0x100A,   // string
    E_PARAM_NET_SIM_IDX       = 0x100B,   // U8, SIM slot
    E_PARAM_SLEEP_TIME        = 0x100C,   // U32,unit: second
    E_PARAM_BACKLIGHT_TIME    = 0x100D,   // U8, unit: second
    E_PARAM_POWEROFF_TIME     = 0x100E,   // U8, unit: minute
    E_PARAM_LANG_TYPE         = 0x100F,   // U8
    E_PARAM_NET_NAME_TMS      = 0x1010,   // string, TMS server address
    E_PARAM_NET_NAME_TMS_PORT = 0x1011,   // string, TMS server port
} E_PARAM_ID;

//-----------------------------------------------------------------------------
// Function declaration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name    : Lib_ParamInit
// Func    : Initialize the parameter cache and load the parameter file
// Paras   :
// Ret     : RET_OK - success, RET_ERR - failure
// Desc    : Call once at startup, before any working thread is created and
//           before Lib_LangInit(). The built-in default values are used when
//           the file does not exist or is invalid, the file is rebuilt then.
//           Calling it more than once has no effect.
//-----------------------------------------------------------------------------
int Lib_ParamInit(void);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamGet
// Func    : Read one parameter from the RAM cache
// Paras   : usParamId[in],     parameter ID, see E_PARAM_ID
//           pucParam[out],     output buffer, at least the size of the type
//           pucParamLen[out],  output length of the field
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID
// Desc    : The caller must provide a buffer of PARAM_RAW_SIZE, PARAM_U8_SIZE,
//           PARAM_U32_SIZE, PARAM_F32_SIZE or PARAM_STR_SIZE according to the
//           parameter type (see the header of each ID). Strings are copied
//           with the ending 0, the output length is the fixed slot size.
//           It never touches the file.
//-----------------------------------------------------------------------------
int Lib_ParamGet(unsigned short usParamId, unsigned char *pucParam, unsigned char *pucParamLen);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSet
// Func    : Update one parameter in the RAM cache
// Paras   : usParamId[in],   parameter ID, see E_PARAM_ID
//           pucParam[in],    input data
//           ucParamLen[in],  length of the input data
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID, wrong length or string too long
// Desc    : Numeric parameters require the exact length, strings and raw
//           bytes accept a short write, the remaining bytes are cleared.
//           Strings must be NUL terminated. The value is not checked against
//           any range. The change only takes effect in RAM, call
//           Lib_ParamSave() to persist it. It never touches the file.
//-----------------------------------------------------------------------------
int Lib_ParamSet(unsigned short usParamId, unsigned char *pucParam, unsigned char ucParamLen);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSave
// Func    : Write the modified persistent parameters to the parameter file
// Paras   :
// Ret     : RET_OK - success or nothing to save
//           RET_ERR - not initialized or file operation failure
// Desc    : Only the parameters which were changed since the last successful
//           save are written, the whole file is rebuilt in one write. Call it
//           when a page is left, before power off or reboot. On failure the
//           dirty flag is kept, the caller may retry later.
//-----------------------------------------------------------------------------
int Lib_ParamSave(void);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamGetInt
// Func    : Read a U8 or U32 parameter as an int
// Paras   : usParamId[in],   parameter ID
//           piValue[out],    output value
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID, unsupported type or U32 > INT_MAX
// Desc    :
//-----------------------------------------------------------------------------
int Lib_ParamGetInt(unsigned short usParamId, int *piValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSetInt
// Func    : Write a U8 or U32 parameter from an int
// Paras   : usParamId[in],   parameter ID
//           iValue[in],      input value, must not be negative
// Ret     : RET_OK - success
//           RET_ERR - not initialized
//           RET_ERR_PARAMETER - unknown ID, unsupported type or negative value
// Desc    : The change only takes effect in RAM, call Lib_ParamSave() to
//           persist it.
//-----------------------------------------------------------------------------
int Lib_ParamSetInt(unsigned short usParamId, int iValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamGetU8
// Func    : Read a U8 parameter
// Paras   : usParamId[in],   parameter ID
//           pucValue[out],   output value
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID or unsupported type
// Desc    :
//-----------------------------------------------------------------------------
int Lib_ParamGetU8(unsigned short usParamId, unsigned char *pucValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSetU8
// Func    : Write a U8 parameter
// Paras   : usParamId[in],   parameter ID
//           ucValue[in],     input value
// Ret     : RET_OK - success
//           RET_ERR - not initialized
//           RET_ERR_PARAMETER - unknown ID or unsupported type
// Desc    : The change only takes effect in RAM, call Lib_ParamSave() to
//           persist it.
//-----------------------------------------------------------------------------
int Lib_ParamSetU8(unsigned short usParamId, unsigned char ucValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamGetU32
// Func    : Read a U8 or U32 parameter as an unsigned long
// Paras   : usParamId[in],   parameter ID
//           pulValue[out],   output value
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID or unsupported type
// Desc    : A U8 value is zero extended.
//-----------------------------------------------------------------------------
int Lib_ParamGetU32(unsigned short usParamId, unsigned int *uiValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSetU32
// Func    : Write a U8 or U32 parameter from an unsigned long
// Paras   : usParamId[in],   parameter ID
//           ulValue[in],     input value
// Ret     : RET_OK - success
//           RET_ERR - not initialized
//           RET_ERR_PARAMETER - unknown ID, unsupported type or value > 0xFF
//                               when the parameter is a U8
// Desc    : The change only takes effect in RAM, call Lib_ParamSave() to
//           persist it.
//-----------------------------------------------------------------------------
int Lib_ParamSetU32(unsigned short usParamId, unsigned int uiValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamGetStr
// Func    : Read a string parameter
// Paras   : usParamId[in],   parameter ID
//           pcBuf[out],      output buffer
//           ucSize[in],      size of the output buffer
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID, unsupported type or buffer too
//                               small (strlen + 1 is required, no truncation)
// Desc    : The output is always NUL terminated.
//-----------------------------------------------------------------------------
int Lib_ParamGetStr(unsigned short usParamId, char *pcBuf, unsigned char ucSize);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSetStr
// Func    : Write a string parameter
// Paras   : usParamId[in],   parameter ID
//           pcStr[in],       input string, must be NUL terminated
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID, unsupported type or the string
//                               does not fit in PARAM_STR_SIZE
// Desc    : The remaining bytes of the slot are cleared. The change only
//           takes effect in RAM, call Lib_ParamSave() to persist it.
//-----------------------------------------------------------------------------
int Lib_ParamSetStr(unsigned short usParamId, const char *pcStr);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamGetFloat
// Func    : Read a F32 parameter
// Paras   : usParamId[in],   parameter ID
//           pfValue[out],    output value
// Ret     : RET_OK - success
//           RET_ERR - not initialized or null pointer
//           RET_ERR_PARAMETER - unknown ID or unsupported type
// Desc    :
//-----------------------------------------------------------------------------
int Lib_ParamGetFloat(unsigned short usParamId, float *pfValue);

//-----------------------------------------------------------------------------
// Name    : Lib_ParamSetFloat
// Func    : Write a F32 parameter
// Paras   : usParamId[in],   parameter ID
//           fValue[in],      input value, NaN is rejected
// Ret     : RET_OK - success
//           RET_ERR - not initialized
//           RET_ERR_PARAMETER - unknown ID, unsupported type or NaN
// Desc    : The value is stored as IEEE754 4 bytes. The change only takes
//           effect in RAM, call Lib_ParamSave() to persist it.
//-----------------------------------------------------------------------------
int Lib_ParamSetFloat(unsigned short usParamId, float fValue);

#endif
