/**********************************************************************************************************
 * File Name     : EmvDemo.h
 * Description   : Compatibility layer for the legacy Dspread POS SDK common header
 *
 *   Supplies the types / macros / declarations missing in this repository for EmvNew/EmvDemo.c and app_emv.c (the EMV L2 kernel adaptation layer),
 *   so that the sample flow and kernel adaptation code under the EmvNew directory can build in the current LVGL App project.
 **********************************************************************************************************/
#ifndef __EMVDEMO_H__
#define __EMVDEMO_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "LibApi.h"     /* Lib_* interfaces / KB_* key codes / ST_MSC_TRACK / TRUE / FALSE */

/* ---------------- Legacy SDK basic types (required by T_U8_VIEW in app_emv.h) ---------------- */
typedef unsigned char   u8;
typedef unsigned char  *pu8;
typedef unsigned short  u16;
typedef unsigned short *pu16;
typedef unsigned int    u32;
typedef unsigned int   *pu32;

/* ---------------- Card detection modes (legacy CardDemo convention) ---------------- */
#define CARD_NONE           0x00
#define SCI_DETECETED       0x01    /* Contact */
#define NFC_DETECETED       0x02    /* Contactless */
#define MSC_DETECETED       0x03    /* Magnetic stripe */

/* ---------------- Magnetic stripe / card info (legacy Card.h compatible) ---------------- */
#define TRACK_NUM           SDK_TRACK_NUM

#define UI_INPUT_CANCEL     0x1001  /* User canceled input */
#define UI_INPUT_TIMEOUT    0x1002  /* Input timeout */

#define MODE_MSC            0x04    /* Magnetic stripe swipe entry mode */

enum
{
    CARD_VISA = 1,
    CARD_MASTERCARD,
    CARD_AMEX,
    CARD_DINERS,
    CARD_JCB,
    CARD_DISCOVER,
    CARD_UNIONPAY,
};

typedef struct
{
    unsigned char eEntryMode;       /* MODE_MSC */
    unsigned char eCardType;        /* CARD_xxx */
    unsigned char aucCardNo[24];    /* PAN ASCII */
    unsigned char ucCardLen;
    unsigned char aucTrack2Data[128];
    unsigned char ucTrack2Len;
    unsigned char aucExpiredDate[8];/* YYMM ASCII */
    unsigned char aucServiceCode[8];
} ST_CARDINFO;

typedef struct
{
    unsigned char aucName[8];
    unsigned char aucResver[4];
    unsigned char ucTrackPro;
} ST_PARAMETER_CONFIG_LIST;

/* ---------------- Transaction data (Emv_Process input, required by app_emv.h) ---------------- */
typedef struct _ST_TRANSDATA
{
    unsigned char trans_amount[12 + 1]; /* Transaction amount n12 ASC, unit: cents */
    unsigned char trans_time[7];        /* Transaction time YYMMDDhhmmss BCD; when all zeros the kernel uses the current time */
    unsigned char trans_type;           /* EMVTransType */
    short         cardDetectType;       /* SCI/NFC/MSC_DETECETED */
    ST_CARDINFO   stCardInfo;           /* Magnetic stripe card parsing result (MccDemo output) */
} ST_TRANSDATA;

#include "app_emv.h"    /* EmvCallBack_t / EMV_L2_Return / EmvOnlineData_t, etc. */

/* ---------------- Lightweight substitutes for legacy UI / crypto dependencies ---------------- */
#define BeepOn()            Lib_Beep(50)

/* The demo project does not encrypt track1: Track1Process falls back to plaintext on non-zero return */
static inline int DataEncryptProcess(unsigned char *in, unsigned char *out, int len, int key)
{
    (void)in;
    (void)out;
    (void)len;
    (void)key;
    return -1;
}

/* Note: user_interface_request_data_t (UIRD) is defined by EmvNew/include/outcome_list.h */

/* ---------------- LVGL key-wait bridge (implemented by EmvNew/emv_pay.c, called by app_emv.c) ----------------
 * Semantics: wait for a user cancel action for at most timeoutSec seconds;
 *           return KB_CANCEL on user cancel, LVGL_OPER_TIMEOUT on timeout. */
#define LVGL_OPER_TIMEOUT   (-1024)
int LvUiWaitKey(int timeoutSec);

/* ---------------- Beep bridge (implemented by emv_pay.c, throttled playback on the LVGL thread) ----------------
 * The EMV worker thread must not call Lib_Beep directly: the audio player would be created/destroyed repeatedly,
 * and cross-thread calls would stall the audio sub-thread and freeze the whole device. */
void EmvPage_Beep(unsigned long ms);

#endif /* __EMVDEMO_H__ */
