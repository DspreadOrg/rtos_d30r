//-----------------------------------------------------------------------------
// @file    : i18n_langs.c
// @brief   : Built-in language registration
// @details : English is always built in and uses the key as text. Other
//            languages are provided by their own file and added with
//            Lib_LangRegister(), for example:
//
//            extern const LIB_LANG_DESC_T g_lang_pt;
//            Lib_LangRegister(&g_lang_pt);
//
// @author  :
// @version :
// @date    :
// @history :
//-----------------------------------------------------------------------------
#include "LibLanguage.h"
#include "app_i18n.h"

// TTF injected into LittleFS by ResourceInit(). It is a Latin-only font, so
// pName of every language must stay renderable with it (use a Latin name for
// languages whose script is not covered). Persian requires a dedicated TTF,
// see i18n_fa.c.
#define LANG_FONT_NAME          APP_FONT_FILE_LATIN

//-----------------------------------------------------------------------------
// Local variable
//-----------------------------------------------------------------------------
static const LIB_LANG_DESC_T s_lang_en =
{
    "English",                  /* pName     : displayed in the language page */
    "en",                       /* pCode     : must be unique */
    0,                          /* ucParam   : value written to E_LANG_TYPE */
    0,                          /* ucRtl     : 0 - left to right */
    LANG_FONT_NAME,             /* pFontName : TTF shared by all built-in languages */
    NULL,                       /* pTable    : NULL - the key is the text */
};

// Defined in i18n_pt.c / i18n_fa.c
extern const LIB_LANG_DESC_T g_lang_pt;
extern const LIB_LANG_DESC_T g_lang_fa;

//-----------------------------------------------------------------------------
// Name    : Lib_LangRegisterBuiltin
// Func    : Register the languages compiled into the firmware
// Paras   :
// Ret     :
// Desc    : Must be called before Lib_LangInit().
//-----------------------------------------------------------------------------
void Lib_LangRegisterBuiltin(void)
{
    Lib_LangRegister(&s_lang_en);
    //Lib_LangRegister(&g_lang_pt);
    Lib_LangRegister(&g_lang_fa);
}
