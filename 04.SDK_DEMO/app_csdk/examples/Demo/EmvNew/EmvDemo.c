/**********************************************************************************************************
 * Copyright(C)2018,
 * File Name     : EmvDemo.c
 * Version       : v1.0
 * Author        :
 * Description   : EMV / MSR application sample flow (adapted for LVGL payment page)
 *
 *   This file is the page-adapted version of the original EmvDemo flow:
 *     - All UI interactions (AID selection / PIN input / status prompts / online / result) are replaced by
 *       the EmvPage_* callbacks provided by EmvNew/emv_pay.c, driven by page_emv.c for display.
 *     - Transaction data (PAN / expiry / application / result) is obtained from kernel TLVs or track parsing.
 *     - The EmvDemo() five-stage flow and the MccDemo() parsing algorithm are preserved.
 **********************************************************************************************************/
/**************************** INCLUDE FILES BEGIN **************************/

#include "EmvDemo.h"
#include <string.h>
#include <stdlib.h>
#include "global.h"
#include "EmvUtils.h"
#include "emv_pay.h"

/**************************** INCLUDE FILES END ****************************/

#define MSC_TMP_BUF_LEN 1000

/* Track data (injected by page_payment.c via EmvDemo_SetMsrTracks()) */
ST_MSC_TRACK astCiTrack[TRACK_NUM];

static unsigned char g_aucPAN[20];
static unsigned char g_ucPANLen;
static unsigned char aucMacPanAscii[30];

ST_PARAMETER_CONFIG_LIST stParConlist;

unsigned char g_aucTradeMsgBuf[1024];
unsigned short g_usTradeMsgLen = 0;

static EMV_L2_Return s_lastEmvRet = APP_RC_EMV_DENAIL;

/* forward declarations */
short EmvDemo(short cardMode, ST_TRANSDATA *pstTransData);
short MccDemo(ST_TRANSDATA *pstTransData);

/*--------------------------------------
|   Function Name: EmvDemo_SetMsrTracks
|   Description  : Copy track data into the EmvNew internal buffer for MccDemo parsing
|   Parameters     : t1/t2/t3 - track 1/2/3
|   Returns        :
+-------------------------------------*/
void EmvDemo_SetMsrTracks(const ST_MSC_TRACK *t1, const ST_MSC_TRACK *t2, const ST_MSC_TRACK *t3)
{
    static unsigned char trackBuf[TRACK_NUM][512];
    const ST_MSC_TRACK *src[TRACK_NUM];
    int i;

    src[SDK_TRACK1] = t1;
    src[SDK_TRACK2] = t2;
    src[SDK_TRACK3] = t3;

    for (i = 0; i < TRACK_NUM; i++)
    {
        memset(&astCiTrack[i], 0, sizeof(astCiTrack[i]));
        if (src[i] == NULL)
            continue;

        astCiTrack[i] = *src[i];
        if (src[i]->ucTrackValid && src[i]->pucTrackBuf != NULL &&
            src[i]->ucTrackLen < sizeof(trackBuf[i]))
        {
            memcpy(trackBuf[i], src[i]->pucTrackBuf, src[i]->ucTrackLen);
            trackBuf[i][src[i]->ucTrackLen] = '\0';
            astCiTrack[i].pucTrackBuf = trackBuf[i];
        }
    }
}

static u8 GetCardType(unsigned char *pucPan)
{
    int iBin = 0;
    unsigned char ucBuf[8] = {0};
    unsigned char ucCardType = 0;

    FUN_IN;
    memcpy(ucBuf, pucPan, 8);
    iBin = atoi((char *)ucBuf);
    TRACE("Bin = %d", iBin);

    if ((40000000 <= iBin) && (iBin <= 49999999))
    {
        ucCardType = CARD_VISA;
    }
    else if (((51000000 <= iBin) && (iBin <= 55999999)) ||
             ((22210000 <= iBin) && (iBin <= 27209999)) ||
             ((58276600 <= iBin) && (iBin <= 58276699)))
    {
        ucCardType = CARD_MASTERCARD;
    }
    else if (((37000000 <= iBin) && (iBin <= 37999999)) ||
             ((34000000 <= iBin) && (iBin <= 34999999)))
    {
        ucCardType = CARD_AMEX;
    }
    else if (((30000000 <= iBin) && (iBin <= 30599999)) ||
             ((30950000 <= iBin) && (iBin <= 30959999)) ||
             ((38000000 <= iBin) && (iBin <= 39999999)))
    {
        ucCardType = CARD_DINERS;
    }
    else if ((35280000 <= iBin) && (iBin <= 35899999))
    {
        ucCardType = CARD_JCB;
    }
    else if (((60110000 <= iBin) && (iBin <= 60110999)) ||
             ((60112000 <= iBin) && (iBin <= 60114999)) ||
             ((60117400 <= iBin) && (iBin <= 60117499)) ||
             ((60117700 <= iBin) && (iBin <= 60117999)) ||
             ((60118600 <= iBin) && (iBin <= 60119999)) ||
             ((62212600 <= iBin) && (iBin <= 62292599)) ||
             ((62400000 <= iBin) && (iBin <= 62699999)) ||
             ((62820000 <= iBin) && (iBin <= 62889999)) ||
             ((64400000 <= iBin) && (iBin <= 65999999)) ||
             ((6210940 <= iBin) && (iBin <= 6210949)))
    {
        ucCardType = CARD_DISCOVER;
    }
    return ucCardType;
}

// card transaction demo
short CardDemo(short cardMode, ST_TRANSDATA *pstTransData)
{
    short errCode;

    switch (cardMode)
    {
    case SCI_DETECETED: // Contact card
    case NFC_DETECETED: // Contacless card
        TRACE("Start EMV Demo!!!\r\n");
        errCode = EmvDemo(cardMode, pstTransData);
        break;

    case MSC_DETECETED:
        // Magnetic stripe cards
        TRACE("Start Mcc Demo!!!\r\n");
        errCode = MccDemo(pstTransData);
        break;

    default:
        // Invalid card
        errCode = 0x8001;
        break;
    }

    TRACE("errCode %x\r\n", errCode);
    return errCode;
}

static signed short searchChar(unsigned char *pBuf, unsigned char ch, signed short startpos)
{
    signed short pos = startpos;

    if (startpos < 0)
    {
        return -1;
    }

    while (*(pBuf + pos) != 0)
    {
        if (*(pBuf + pos) == ch)
        {
            return pos;
        }
        pos++;
    }
    return -1;
}

// service code
static u8 GetCardSevCode(unsigned char *pucTrackBuf, unsigned char *pucCardSevCode, unsigned char *pucCardSevCodeLen)
{
    signed short sOffSet1;
    unsigned char *pucTrack2Buf;
    unsigned char ucLen;

    pucTrack2Buf = pucTrackBuf;

    // Find the position of the equal sign
    sOffSet1 = searchChar(pucTrack2Buf, '=', 0);
    if (sOffSet1 < 0)
    {
        return FALSE;
    }

    //+1, starting position of validity period
    sOffSet1++;

    //+4, starting position of the service code
    sOffSet1 += 4;

    Lib_AscNum2bcd(pucTrack2Buf + sOffSet1, pucCardSevCode, 3, &ucLen, 0x0f);
    *pucCardSevCodeLen = ucLen;
    return TRUE;
}

/*--------------------------------------
|   Function Name: Initialize_EMV_CallBackFun
|   Description  : Page-adapted version of the original callback table:
|                  all interactions are handled by page_emv.c (via EmvPage_*)
+-------------------------------------*/
static void Initialize_EMV_CallBackFun(EmvCallBack_t *pcallbackfun)
{
    pcallbackfun->EMV_AidSelect = EmvPage_SelectApp;     /* was EmvSelectApp */
    pcallbackfun->EMV_SetKernelId = NULL;
    pcallbackfun->EMV_ConfirmCardInfo = NULL;
    pcallbackfun->EMV_InputPasswd = EmvPage_InputPin;    /* was EmvInputPin */
    pcallbackfun->EMV_CertConfirm = NULL;
    pcallbackfun->EMV_ProcessDisp = EmvPage_ProcessDisp; /* was EmvProcessDisp */
    pcallbackfun->EMV_OnlineProcess = EmvPage_OnlineProcess; /* was onlineProcess */
    pcallbackfun->EMV_AfterSelectApp = NULL;
    pcallbackfun->EMV_AfterReadRecord = NULL;
}

void EmvL2_Init(void)
{
    EmvCallBack_t t_callbackfun;
    Initialize_EMV_CallBackFun(&t_callbackfun);
    Emv_KernelInit(t_callbackfun);
}

EMV_L2_Return EmvDemo_GetLastResult(void)
{
    return s_lastEmvRet;
}

short EmvDemo(short cardMode, ST_TRANSDATA *pstTransData)
{
    FUN_IN;
    int ret = -1;
    EMV_L2_Return nEmvRet = APP_RC_EMV_DENAIL;

    EmvL2_Init();
    /* Amount is filled into trans_amount by the caller (payment page input) */
    pstTransData->cardDetectType = cardMode;
    pstTransData->trans_type = EMV_L2_SALE;
    nEmvRet = Emv_Process(pstTransData);

    if (cardMode == NFC_DETECETED && nEmvRet == APP_RC_COMPLETED)
    {
        // do contactless trans online request
        EmvOnlineData_t emvOnlineData;
        memset(&emvOnlineData, 0, sizeof(EmvOnlineData_t));
        ret = EmvPage_OnlineProcess(&emvOnlineData);
        if (ret == 0 && memcmp(emvOnlineData.iccResponse, "00", 2) == 0)
        {
            /* Only RuPay CSU that brings back host Issuer Data (ackdata) goes through the second tap;
               the simulated online response has no ackdata (CheckDoubleTap always returns 0), so it is approved directly,
               which also avoids dumping the transaction pool again inside that function (333.trc hangs non-deterministically here) */
            if (emvOnlineData.ackdatalen != 0 && Emv_CheckContactlessDoubleTap(&emvOnlineData))
                ret = Emv_SetContactlessOnlineResult(&emvOnlineData);
            if (ret == 0)
                nEmvRet = APP_RC_COMPLETED;
            else
                nEmvRet = APP_RC_EMV_DENAIL;
        }
        else
        {
            nEmvRet = APP_RC_EMV_DENAIL;
        }
    }

    s_lastEmvRet = nEmvRet;
    return (nEmvRet == APP_RC_COMPLETED) ? 0 : -1;
}

// valide data
u8 GetCardValidity(unsigned char *pucTrackBuf, unsigned char *pucCardValidity, unsigned char *pucCardValidityLen)
{
    signed short sOffSet1;
    unsigned char *pucTrack2Buf;
    unsigned char ucLen;

    pucTrack2Buf = pucTrackBuf;

    // Find the position of the equal sign
    sOffSet1 = searchChar(pucTrack2Buf, '=', 0);
    if (sOffSet1 < 0)
    {
        return FALSE;
    }

    //+1, Starting bit of validity period
    sOffSet1++;

    Lib_AscNum2bcd(pucTrack2Buf + sOffSet1, pucCardValidity, 4, &ucLen, 0x0f);
    *pucCardValidityLen = ucLen;
    return TRUE;
}

// PAN
static void GetPANfromTrackData(unsigned char *pucTrackBuf, unsigned char *pucCardId, unsigned char *pucCardIdLen)
{
    signed short sOffSet1, sOffSet2;
    unsigned char *pucTrack2Buf;

    pucTrack2Buf = pucTrackBuf;

    // Find the location of the ';' semicolon
    sOffSet1 = searchChar(pucTrack2Buf, ';', 0);
    if (sOffSet1 < 0)
    {
        return;
    }

    // Find the position of the equal sign
    sOffSet2 = searchChar(pucTrack2Buf, '=', sOffSet1);
    if (sOffSet2 < 0)
    {
        return;
    }

    // +1, starting position of the main account
    sOffSet1++;
    // The length of the main account
    sOffSet2 -= sOffSet1;

    if (sOffSet2 < 13 || sOffSet2 > 19)
    {
        // The length of the main account cannot be less than 13 and cannot be greater than 19
        return;
    }

    // As calculating pinblock, pan is used, so the PAN character is stored in the global variable
    memcpy(g_aucPAN, pucTrack2Buf + sOffSet1, sOffSet2);
    g_ucPANLen = sOffSet2;

    memcpy(aucMacPanAscii + 1, g_aucPAN, g_ucPANLen);
    aucMacPanAscii[0] = g_ucPANLen;

    // CUT PAN ,change to bcd, if PAN is not even, the last digit will be filled with an F
    Lib_AscNum2bcd(pucTrack2Buf + sOffSet1, pucCardId, sOffSet2, pucCardIdLen, 0x0f);
}

//-----------------------------------------------------------------------------
// Name    : Track1Process
// Func    : 1th track data processing
// Paras   : pucSource[in],         Source data
//         : ucSourceLen[in],       Source data length
//         : pucDest[out],          Destination Data
//         : pucDestLen[out],       Destination Data length
//         : ucProcess[in],         Magnetic track data processing method
// Ret     : - 0 , success
//           - <0, failure
// Desc    :
//-----------------------------------------------------------------------------
void Track1Process(u8 *pucSource, u8 ucSourceLen, u8 *pucDest, u8 *pucDestLen, u8 ucProcess)
{
    u8 aucBuf[128], ucLen = 0;
    u8 ucKeyIndex;

    ucKeyIndex = stParConlist.aucResver[2];

    TRACE("Data length of track 1 : %d\r\n", ucSourceLen);
    TRACEBUF(pucSource, ucSourceLen);

    // 1th track data processing
    switch (ucProcess)
    {
    case 1:
        // plaintext
        memcpy(pucDest, pucSource, ucSourceLen);
        *pucDestLen = ucSourceLen;
        return;

    case 2:
        // padded with 00 to a multiple of 8
        memcpy(aucBuf, pucSource, ucSourceLen);
        ucLen = ucSourceLen;
        while (ucLen % 8)
        {
            aucBuf[ucLen++] = 0x00;
        }
        break;

    default:
        // plaintext
        memcpy(pucDest, pucSource, ucSourceLen);
        *pucDestLen = ucSourceLen;
        return;
    }
    TRACE("Data length of track 1 after padding : %d", ucLen);
    TRACEBUF(aucBuf, ucLen);

    if (0 != DataEncryptProcess(aucBuf, pucDest, ucLen, ucKeyIndex))
    {
        memcpy(pucDest, aucBuf, ucLen);
    }
    *pucDestLen = ucLen;
    TRACE("Data length of track 1 after encryption : %d\r\n", *pucDestLen);
    TRACEBUF(pucDest, *pucDestLen);

    return;
}

/*--------------------------------------
|   Function Name: InputPIN
|   Description  : MSR transaction PIN input (original keypad scan replaced by the on-page keypad)
|   Returns        : RET_OK / UI_INPUT_CANCEL / UI_INPUT_TIMEOUT
+-------------------------------------*/
short InputPIN(ST_TRANSDATA *pstTransData)
{
    (void)pstTransData;
    return EmvPage_MsrInputPin();
}

short MccDemo(ST_TRANSDATA *pstTransData)
{
    unsigned char aucBuf[512];
    unsigned char ucLen = 0, ucTmpLen = 0;
    short errCode = 0;
    u8 aucTmpBuf[SDK_TRACK_NUM][MSC_TMP_BUF_LEN];
    ST_CARDINFO *pstCardInfo = NULL;
    short sRet = 0;
    FUN_IN;

    pstCardInfo = &pstTransData->stCardInfo;

    signed short sOffSet1 = 0, sOffSet2 = 0;

    if (!astCiTrack[SDK_TRACK2].ucTrackValid)
    {
        TRACE("msr: track2 invalid flag");
        errCode = 0x8004;
        return errCode;
    }

    /* On abnormal track length (0/1/2) the subsequent ucTrackLen-2 underflows to a huge length, so fail directly */
    if (astCiTrack[SDK_TRACK2].pucTrackBuf == NULL || astCiTrack[SDK_TRACK2].ucTrackLen < 3)
    {
        TRACE("FATAL: msr track2 buf=%lx len=%d",
              (unsigned long)astCiTrack[SDK_TRACK2].pucTrackBuf,
              astCiTrack[SDK_TRACK2].ucTrackLen);
        errCode = 0x8004;
        return errCode;
    }

    TRACE("TRACK2 length = %d, TRACK3 length = %d\r\n", astCiTrack[SDK_TRACK2].ucTrackLen, astCiTrack[SDK_TRACK3].ucTrackLen);
    TRACE("TRACK2 = %s, TRACK3 = %s\r\n", astCiTrack[SDK_TRACK2].pucTrackBuf, astCiTrack[SDK_TRACK3].pucTrackBuf);

    pstCardInfo->eEntryMode = MODE_MSC;
    memset(aucBuf, 0, sizeof(aucBuf));
    // get PAN
    ucLen = 0;
    GetPANfromTrackData(astCiTrack[SDK_TRACK2].pucTrackBuf, aucBuf, &ucLen);
    TRACE("msr: pan bcd len=%d", ucLen);
    if (ucLen == 0)
    {
        TRACE("FATAL: msr pan len 0, track2 format invalid");
        errCode = 0x8004;
        return errCode;
    }
    if (ucLen > (unsigned char)(sizeof(pstCardInfo->aucCardNo) / 2 - 1))
    {
        TRACE("FATAL: msr pan len=%d too big, clamp", ucLen);
        ucLen = sizeof(pstCardInfo->aucCardNo) / 2 - 1;
    }
    Lib_Bcd2Asc(aucBuf, (char *)pstCardInfo->aucCardNo, ucLen);
    pstCardInfo->aucCardNo[ucLen * 2] = '\0';
    TRACE("CardNo: %s", pstCardInfo->aucCardNo);

    // remove padding F
    while (pstCardInfo->aucCardNo[0] != '\0' &&
           pstCardInfo->aucCardNo[strlen((const char *)pstCardInfo->aucCardNo) - 1] > '9')
    {
        pstCardInfo->aucCardNo[strlen((const char *)pstCardInfo->aucCardNo) - 1] = '\0';
    }
    pstCardInfo->ucCardLen = strlen((char *)pstCardInfo->aucCardNo);
    pstCardInfo->eCardType = GetCardType(pstCardInfo->aucCardNo);
    TRACE("msr: cardNo=%s len=%d type=%d", pstCardInfo->aucCardNo,
          pstCardInfo->ucCardLen, pstCardInfo->eCardType);

    memset(aucBuf, 0, sizeof(aucBuf));

    // Patch: Malaysia GHL track data format is : track1 | track2 | track3
    if (0 == memcmp(stParConlist.aucName, "\x47\x48\x4C\x00", 4))
    {
        ucTmpLen = 0;

        // 1th track
        if (astCiTrack[SDK_TRACK1].ucTrackValid)
        {
            // Find the position of the %
            sOffSet1 = searchChar(astCiTrack[SDK_TRACK1].pucTrackBuf, '%', 0);
            // Find the position of the ?
            sOffSet2 = searchChar(astCiTrack[SDK_TRACK1].pucTrackBuf, '?', 0);
            if ((sOffSet1 >= 0) && (sOffSet2 >= 0) && (sOffSet2 > sOffSet1))
            {
                sOffSet1++;
                ucLen = sOffSet2 - sOffSet1;
                memcpy(aucTmpBuf[SDK_TRACK2] + ucTmpLen, astCiTrack[SDK_TRACK1].pucTrackBuf + sOffSet1, ucLen);
                ucTmpLen += ucLen;
            }
        }

        // Separator
        aucTmpBuf[SDK_TRACK2][ucTmpLen++] = '|';

        // 2th track
        memcpy(aucTmpBuf[SDK_TRACK2] + ucTmpLen, astCiTrack[SDK_TRACK2].pucTrackBuf + 1, astCiTrack[SDK_TRACK2].ucTrackLen - 2);
        ucTmpLen += astCiTrack[SDK_TRACK2].ucTrackLen - 2;

        // Separator
        aucTmpBuf[SDK_TRACK2][ucTmpLen++] = '|';

        // 3th track
        if (astCiTrack[SDK_TRACK3].ucTrackValid)
        {
            memcpy(aucTmpBuf[SDK_TRACK2] + ucTmpLen, astCiTrack[SDK_TRACK3].pucTrackBuf + 1, astCiTrack[SDK_TRACK3].ucTrackLen - 2);
            ucTmpLen += astCiTrack[SDK_TRACK3].ucTrackLen - 2;
        }
    }
#if 0
    else
    {
        // 2th track data, ucTrackLen includes; And ? length, ucTrackLen-2 has been removed; And ? , just leave only data
        Lib_AscNum2bcd(astCiTrack[SDK_TRACK2].pucTrackBuf + 1, aucTmpBuf[SDK_TRACK2], astCiTrack[SDK_TRACK2].ucTrackLen - 2, &ucTmpLen, 0x0f);
    }
    if (1 != TrackDataProcess(aucTmpBuf[SDK_TRACK2], ucTmpLen, aucTmpBuf[SDK_TRACK3], &ucLen, (u8)SDK_TRACK2, stParConlist.ucTrackPro))
    {
        errCode = 0x8025;
        g_usTradeMsgLen = 0;
        return errCode;
    }
    g_aucTradeMsgBuf[g_usTradeMsgLen] = 0x02;
    g_usTradeMsgLen++;
    g_aucTradeMsgBuf[g_usTradeMsgLen] = ucLen;
    g_usTradeMsgLen++;
    memcpy(g_aucTradeMsgBuf + g_usTradeMsgLen, aucTmpBuf[SDK_TRACK3], ucLen);
    g_usTradeMsgLen += ucLen;

    // 3th track
    if (astCiTrack[SDK_TRACK3].ucTrackValid)
    {
        Lib_AscNum2bcd(astCiTrack[SDK_TRACK3].pucTrackBuf + 1, aucTmpBuf[SDK_TRACK2], astCiTrack[SDK_TRACK3].ucTrackLen - 2, &ucTmpLen, 0x0f);
        if (1 != TrackDataProcess(aucTmpBuf[SDK_TRACK2], ucTmpLen, aucTmpBuf[SDK_TRACK3], &ucLen, (u8)SDK_TRACK3, stParConlist.ucTrackPro))
        {
            errCode = 0x8025;
            g_usTradeMsgLen = 0;
            return errCode;
        }
        g_aucTradeMsgBuf[g_usTradeMsgLen] = 0x03;
        g_usTradeMsgLen++;
        g_aucTradeMsgBuf[g_usTradeMsgLen] = ucLen;
        g_usTradeMsgLen++;
        memcpy(g_aucTradeMsgBuf + g_usTradeMsgLen, aucTmpBuf[SDK_TRACK3], ucLen);
        g_usTradeMsgLen += ucLen;
    }
    else
    {
        if (5 == stParConlist.ucTrackPro)
        {
            // After formatting, it needs to be returned
            // 56 bytes 0XFF
            memset(aucTmpBuf[SDK_TRACK2], 0xFF, sizeof(aucTmpBuf[SDK_TRACK2]));
            if (1 != TrackDataProcess(aucTmpBuf[SDK_TRACK2], 56, aucTmpBuf[SDK_TRACK3], &ucLen, (u8)SDK_TRACK3, stParConlist.ucTrackPro))
            {
                errCode = 0x8025;
                g_usTradeMsgLen = 0;
                return errCode;
            }
            g_aucTradeMsgBuf[g_usTradeMsgLen] = 0x03;
            g_usTradeMsgLen++;
            g_aucTradeMsgBuf[g_usTradeMsgLen] = ucLen;
            g_usTradeMsgLen++;
            memcpy(g_aucTradeMsgBuf + g_usTradeMsgLen, aucTmpBuf[SDK_TRACK3], ucLen);
            g_usTradeMsgLen += ucLen;
        }
    }
#endif
    else
    {
        memset(aucBuf, 0, sizeof(aucBuf));
        memcpy(aucBuf, astCiTrack[SDK_TRACK2].pucTrackBuf + 1, astCiTrack[SDK_TRACK2].ucTrackLen - 2);

        for (int i = 0; i < astCiTrack[SDK_TRACK2].ucTrackLen - 2; i++)
        {
            // '=' convert to D
            if (aucBuf[i] == 0x3D)
            {
                aucBuf[i] = 0x44;
            }
        }
    }
    {
        unsigned int t2len = (unsigned int)astCiTrack[SDK_TRACK2].ucTrackLen - 2;

        if (t2len > sizeof(pstCardInfo->aucTrack2Data) - 1)
        {
            TRACE("FATAL: msr track2 len=%u > %u, clamp", t2len,
                  (unsigned int)sizeof(pstCardInfo->aucTrack2Data) - 1);
            t2len = sizeof(pstCardInfo->aucTrack2Data) - 1;
        }
        memcpy(pstCardInfo->aucTrack2Data, aucBuf, t2len);
        pstCardInfo->ucTrack2Len = (unsigned char)t2len;
    }

    memset(aucBuf, 0, sizeof(aucBuf));
    // card valid date
    ucLen = 0;
    GetCardValidity(astCiTrack[SDK_TRACK2].pucTrackBuf, aucBuf, &ucLen);
    if (ucLen == 0 || ucLen > 2)
    {
        TRACE("msr: validity len=%d -> use 0000", ucLen);
        ucLen = 2;
    }
    Lib_Bcd2Asc(aucBuf, (char *)pstCardInfo->aucExpiredDate, ucLen);
    pstCardInfo->aucExpiredDate[ucLen * 2] = '\0';
    TRACE("Card ExpiredDate: %s", pstCardInfo->aucExpiredDate);

    memset(aucBuf, 0, sizeof(aucBuf));
    // Get service code
    ucLen = 0;
    GetCardSevCode(astCiTrack[SDK_TRACK2].pucTrackBuf, aucBuf, &ucLen);
    if (ucLen == 0 || ucLen > 2)
    {
        TRACE("msr: service code len=%d -> use 000", ucLen);
        ucLen = 2;
    }
    Lib_Bcd2Asc(aucBuf, (char *)pstCardInfo->aucServiceCode, ucLen);
    pstCardInfo->aucServiceCode[ucLen * 2] = '\0';
    TRACE("Card ServiceCode: %s", pstCardInfo->aucServiceCode);

    if (astCiTrack[SDK_TRACK1].ucTrackValid)
    {
        // 1th track
        //  Find the position of the %
        sOffSet1 = searchChar(astCiTrack[SDK_TRACK1].pucTrackBuf, '%', 0);
        if (sOffSet1 < 0)
        {
            errCode = 0x8025;
            g_usTradeMsgLen = 0;
            return errCode;
        }

        // Find the position of the ?
        sOffSet2 = searchChar(astCiTrack[SDK_TRACK1].pucTrackBuf, '?', sOffSet1);
        if (sOffSet2 < 0)
        {
            errCode = 0x8025;
            g_usTradeMsgLen = 0;
            return errCode;
        }
        TRACE("TRACK1 length = %d, sOffSet1 = %d, sOffSet2 = %d\r\n", astCiTrack[SDK_TRACK1].ucTrackLen, sOffSet1, sOffSet2);

        sOffSet1++;
        // 1th track data length
        sOffSet2 -= sOffSet1;
        Track1Process(astCiTrack[SDK_TRACK1].pucTrackBuf + sOffSet1, sOffSet2, aucTmpBuf[SDK_TRACK3], &ucLen, stParConlist.aucResver[1]);
    }

    sRet = InputPIN(pstTransData);

    return sRet;
}
