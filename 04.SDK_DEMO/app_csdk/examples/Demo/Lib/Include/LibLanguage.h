//-----------------------------------------------------------------------------
// @file    : LibLanguage.h
// @brief   : Multi-language (i18n) functions
// @details : English is built in; other languages are registered at runtime.
//            The English source text is used as the lookup key.
// @author  :
// @version :
// @date    :
// @history :
//-----------------------------------------------------------------------------
#ifndef _LIB_LANGUAGE_H_
#define _LIB_LANGUAGE_H_

#include "LibTypedef.h"

//-----------------------------------------------------------------------------
// Macro definition
//-----------------------------------------------------------------------------

// Language handle. Handle 0 is always the built-in English.
typedef unsigned char LIB_LANG_T;

#define LIB_LANG_EN             ((LIB_LANG_T)0)     // built-in English
#define LIB_LANG_INVALID        ((LIB_LANG_T)0xFF)  // invalid handle

// Maximum registered languages, including the built-in English
#ifndef LIB_LANG_MAX_COUNT
#define LIB_LANG_MAX_COUNT      8
#endif

// Short form of Lib_LangTr()
#define LIB_TR(key)             Lib_LangTr(key)

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

// One entry: pKey is the English source text, pText is the translation.
// pText may be NULL, then the English key is displayed.
typedef struct
{
    const char *pKey;
    const char *pText;
} LIB_LANG_ENTRY_T;

// Translation table of one language. usCount is calculated by sizeof() at
// the definition site, so it never needs to be maintained by hand.
typedef struct
{
    const LIB_LANG_ENTRY_T *pEntries;
    unsigned short          usCount;
} LIB_LANG_TABLE_T;

// Language description. It must stay resident after registration
// (static const data or permanent RAM), the library only stores its address.
typedef struct
{
    const char             *pName;      // native name, e.g. "Portugues"
    const char             *pCode;      // ISO 639-1 code, must be unique
    unsigned char           ucParam;    // value written to E_LANG_TYPE, unique
    unsigned char           ucRtl;      // 1 - right to left, 0 - left to right
    const char             *pFontName;  // TTF file name, NULL or "" - use the current default font
    const LIB_LANG_TABLE_T *pTable;     // NULL - key is the text (English)
} LIB_LANG_DESC_T;

//-----------------------------------------------------------------------------
// Function declaration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name    : Lib_LangRegister
// Func    : Register one language
// Paras   : pDesc[in],  language description, must stay resident
// Ret     : language handle, LIB_LANG_INVALID on failure (table full, invalid
//           description, duplicated code or param)
// Desc    : Registering the same description again returns the existing
//           handle. Call it before Lib_LangInit() so that the saved language
//           can be resolved at startup.
//-----------------------------------------------------------------------------
LIB_LANG_T Lib_LangRegister(const LIB_LANG_DESC_T *pDesc);

//-----------------------------------------------------------------------------
// Name    : Lib_LangRegisterBuiltin
// Func    : Register all the languages built into the firmware
// Paras   :
// Ret     :
// Desc    : Implemented by the application (i18n_langs.c). Call it before
//           Lib_LangInit().
//-----------------------------------------------------------------------------
void Lib_LangRegisterBuiltin(void);

//-----------------------------------------------------------------------------
// Name    : Lib_LangInit
// Func    : Restore the saved language, initialize the runtime language
// Paras   :
// Ret     :
// Desc    : Call once at startup, after Lib_LangRegisterBuiltin().
//-----------------------------------------------------------------------------
void Lib_LangInit(void);

//-----------------------------------------------------------------------------
// Name    : Lib_LangGetCurrent
// Func    : Get the current runtime language
// Paras   :
// Ret     : language handle
// Desc    :
//-----------------------------------------------------------------------------
LIB_LANG_T Lib_LangGetCurrent(void);

//-----------------------------------------------------------------------------
// Name    : Lib_LangSet
// Func    : Switch the runtime language, it takes effect immediately
// Paras   : tLang[in],  language handle
// Ret     : 1 - success, 0 - failure (the language is not registered)
// Desc    : Saves the language to E_LANG_TYPE (and its font to E_FONT_NAME when
//           pFontName is set), then switches the runtime language. It does not
//           touch LVGL, the caller must rebuild the page afterwards.
//-----------------------------------------------------------------------------
unsigned char Lib_LangSet(LIB_LANG_T tLang);

//-----------------------------------------------------------------------------
// Name    : Lib_LangGetCount
// Func    : Get the number of registered languages
// Paras   :
// Ret     : language count, the built-in English is included
// Desc    :
//-----------------------------------------------------------------------------
unsigned short Lib_LangGetCount(void);

//-----------------------------------------------------------------------------
// Name    : Lib_LangGetAt
// Func    : Get the handle of the language at the given index
// Paras   : usIndex[in],  index in registration order
// Ret     : language handle, LIB_LANG_INVALID if out of range
// Desc    : Used by the language page to walk all the languages.
//-----------------------------------------------------------------------------
LIB_LANG_T Lib_LangGetAt(unsigned short usIndex);

//-----------------------------------------------------------------------------
// Name    : Lib_LangGetName
// Func    : Get the native name of a language
// Paras   : tLang[in],  language handle
// Ret     : name string, "" for an invalid handle
// Desc    : The returned string is static, do not modify or free it.
//-----------------------------------------------------------------------------
const char *Lib_LangGetName(LIB_LANG_T tLang);

//-----------------------------------------------------------------------------
// Name    : Lib_LangIsRtl
// Func    : Check whether a language is written from right to left
// Paras   : tLang[in],  language handle
// Ret     : 1 - right to left, 0 - left to right
// Desc    : Reserved for the RTL layout of a later version.
//-----------------------------------------------------------------------------
unsigned char Lib_LangIsRtl(LIB_LANG_T tLang);

//-----------------------------------------------------------------------------
// Name    : Lib_LangTr
// Func    : Translate a key with the current runtime language
// Paras   : pKey[in],  English source text, must be a literal or static string
// Ret     : translated text, the key itself if it is not translated
// Desc    : The returned string is static, do not modify or free it.
//-----------------------------------------------------------------------------
const char *Lib_LangTr(const char *pKey);

//-----------------------------------------------------------------------------
// Name    : Lib_LangTrGet
// Func    : Translate a key with the given language
// Paras   : tLang[in],  language handle
//           pKey[in],   English source text
// Ret     : translated text, the key itself if it is not translated
// Desc    : Used by the language page for preview.
//-----------------------------------------------------------------------------
const char *Lib_LangTrGet(LIB_LANG_T tLang, const char *pKey);

//-----------------------------------------------------------------------------
// Name    : Lib_LangFormat
// Func    : Translate a key and format it with parameters
// Paras   : pBuf[out],   output buffer
//           usSize[in],  size of the buffer
//           pKey[in],    English source text which contains format specifiers
//           ...          parameters
// Ret     : pBuf
// Desc    : Only for dynamic text, pure numbers and versions do not need it.
//-----------------------------------------------------------------------------
const char *Lib_LangFormat(char *pBuf, unsigned short usSize, const char *pKey, ...);

#endif
