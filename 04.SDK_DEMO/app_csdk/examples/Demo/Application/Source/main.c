/**********************************************************************************************************
 * File Name     : main.c
 *  Version      :
 *  Author       :
 * Description   : App main function
 **********************************************************************************************************/

#include "version.h"
#include "LibApi.h"
#include "LibLanguage.h"
#include "LibParamManage.h"

#include "global.h"
#include "app_main.h"
#include "app_tms.h"

//To get the version data after renaming the compile script, define this string
const char SWVerStr[] = {"\nVER:"INFO_DEV_SVER};

int main(void)
{
    // Trace log configuration, must be called before TRACE
    Lib_TraceLogConfig(1);
    Lib_Trace("enter main");
    lv_log_register_print_cb((lv_log_print_g_cb_t)Lib_Trace);

    // System initialization, must be called
    Lib_SysInit();

    // Lang initialization, must be called
    // Lib_LangInit() restores the language saved in E_LANG_TYPE. Do not call
    // Lib_LangSet() here, it would persist English again and lose the choice.
    Lib_LangRegisterBuiltin();
    Lib_LangInit();
    Lib_LangSet(0);

    // Lvgl initialization, when use lvgl, must be called
    Lib_LvglInit();

    // App initialization, must be called
    Lib_AppInit();

    /* Parameter module: runs before language init and worker thread creation; built-in defaults are used when the file is invalid */
    Lib_ParamInit();

    /* Initialize TMS upgrade parameters (version record / breakpoint info) */
    Lib_TmsParamInit();

    AppStart();

    return 0;
}

int appimg_enter(void *argv)
{
    // The parameter is OS API
    Lib_OsApiInit(argv);

    main();

    return 0;
}

void appimg_exit(void)
{
    TRACE("External MAINAPP exit");
}


