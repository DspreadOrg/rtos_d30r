/**********************************************************************************************************
 * File Name     : global.h
 * Description   : Global common definitions
 **********************************************************************************************************/

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#define FUN_IN                                                                                                         \
    {                                                                                                                  \
        TRACE("@FUN_IN--->%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);                                              \
    }

#define TRACE(...)        Lib_Trace(__VA_ARGS__)
#define TRACEBUF(b, l)    Lib_TraceBuf((unsigned char *)(b), (unsigned short)(l))
#define LOG_DEBUG         0
#define LOG_ERROR         1
#define OsLog(level, ...) Lib_Trace(__VA_ARGS__)

#endif /* __GLOBAL_H__ */
