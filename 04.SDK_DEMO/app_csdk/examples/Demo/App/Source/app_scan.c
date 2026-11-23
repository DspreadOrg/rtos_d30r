/**********************************************************************************************************
 * File Name     : app_scan.c
 * Description   : Scan service, trimmed from the camera part of the old BarcodeTrade.c (removing transaction/payment semantics)
 **********************************************************************************************************/

#include "app_scan.h"
#include "app_ui.h"
#include "LibSysapi.h"
#include "LibCamera.h"

#include <stdlib.h>
#include <string.h>

#define QR_RESULT_HEAD          "+QCAMPRES: 0,"
#define QR_RESULT_MIN_LEN       19

static unsigned char s_opened = 0;

/* Parse scan results in the "+QCAMPRES: 0,18,8,XXXXXXXX" format */
static unsigned char scan_parse(const char *data, char *qrCode, unsigned long dataLen, unsigned long *outLen)
{
    unsigned long offset = 0;
    unsigned long headLen = strlen(QR_RESULT_HEAD);
    char *head = NULL;
    char *start = NULL;
    char *firstSeq = NULL;
    char *secSeq = NULL;
    char lenText[8] = {0};
    unsigned long len = 0;
    unsigned long copyLen = 0;

    if (data == NULL || qrCode == NULL)
    {
        return FALSE;
    }

    head = strstr(data, QR_RESULT_HEAD);
    if (head == NULL)
    {
        return FALSE;
    }

    start = head + headLen;
    offset += headLen;

    firstSeq = strstr(start, ",");
    if (firstSeq == NULL)
    {
        return FALSE;
    }
    offset += (unsigned long)(firstSeq - start) + 1;

    secSeq = strstr(firstSeq + 1, ",");
    if (secSeq == NULL)
    {
        return FALSE;
    }

    len = (unsigned long)(secSeq - firstSeq);
    if (len > 7)
    {
        return FALSE;
    }

    memcpy(lenText, firstSeq + 1, len);
    copyLen = (unsigned long)atoi(lenText);

    offset += (unsigned long)(secSeq - firstSeq);
    if (copyLen > dataLen - offset)
    {
        return FALSE;
    }

    *outLen = copyLen;
    memmove(qrCode, secSeq + 1, copyLen);
    qrCode[copyLen] = 0;
    return TRUE;
}

int app_scan_start(void)
{
    if (s_opened)
    {
        return RET_OK;
    }

    Lib_CamLed(CAM_LED_ENABLE);
    Lib_CamInit();
    Lib_CamOpen();
    //Lib_CamDisp(TRUE);
    s_opened = 1;
    return RET_OK;
}

int app_scan_stop(void)
{
    if (!s_opened)
    {
        return RET_OK;
    }

    Lib_CamLed(CAM_LED_DISABLE);
    Lib_CamClose();
    //Lib_CamDisp(FALSE);
    s_opened = 0;
    return RET_OK;
}

int app_scan_poll(unsigned char *buf, unsigned short *plen)
{
    int rc;
    unsigned long outLen = 0;

    if (buf == NULL || plen == NULL || *plen < QR_RESULT_MIN_LEN)
    {
        return RET_ERR;
    }

    rc = Lib_CamGetResult(buf, *plen);
    if (rc <= 0)
    {
        return (rc < 0) ? RET_ERR : 0;
    }

    if (!scan_parse((char *)buf, (char *)buf, (unsigned long)rc, &outLen))
    {
        /* Abnormal result format; reopen the camera and continue scanning */
        Lib_CamOpen();
        return 0;
    }

    *plen = (unsigned short)outLen;
    Lib_Beep(BEEP_TIME_DEF);               /* Scan success beep */
    return 1;
}
