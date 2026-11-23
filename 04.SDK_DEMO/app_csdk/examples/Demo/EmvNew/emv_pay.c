//-----------------------------------------------------------------------------
// @file    : emv_pay.c
// @brief   : EMV / MSR payment flow (port of EmvNew/EmvDemo.c entry logic)
// @details : Worker thread calls CardDemo(cardMode, &transData), which runs:
//              EMV  : EmvDemo()  -> Emv_KernelInit -> Emv_Process ->
//                                   contactless double tap -> result
//              MSR  : MccDemo()  -> track2 parsing -> page PIN -> ST_CARDINFO
//            EmvDemo.c registers EmvPage_* callbacks; they block on the shared
//            context until the LVGL page (page_emv.c) answers.
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "emv_pay.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
#define EMV_PAY_STACK_SIZE       (128 * 1024)
#define EMV_PAY_POLL_MS          20
#define EMV_PAY_AID_TIMEOUT_SEC  60
#define EMV_PAY_PIN_TIMEOUT_SEC  30
#define EMV_PAY_CONFIRM_SEC      30

//-----------------------------------------------------------------------------
// Locals
//-----------------------------------------------------------------------------
static emv_pay_ctx_t s_ctx;
static short s_cardMode = SCI_DETECETED;
static volatile int s_cancelReq = 0;

/* Round-888 diagnostics: EMV worker stack watermark base/lowest watermark (updated by the probe in app_emv.c) */
unsigned long g_emvStackTop = 0;
unsigned long g_emvStackMin = 0;

//-----------------------------------------------------------------------------
// Local functions: request/answer bridge
//-----------------------------------------------------------------------------
static void emv_pay_post_req(emv_pay_req_t req)
{
    s_ctx.answer = -1;
    s_ctx.answerReady = 0;
    s_ctx.req = req;
    s_ctx.state = EMV_PAY_ST_WAIT_PAGE;
}

static void emv_pay_end_req(void)
{
    s_ctx.req = EMV_PAY_REQ_NONE;
    s_ctx.state = EMV_PAY_ST_RUNNING;
}

// returns 1 = answered, 0 = cancelled/timeout
static int emv_pay_wait_answer(int timeoutSec)
{
    int waited = 0;
    while (waited < timeoutSec * 1000)
    {
        if (s_cancelReq)
            return 0;
        if (s_ctx.answerReady)
            return 1;
        Lib_DelayMs(EMV_PAY_POLL_MS);
        waited += EMV_PAY_POLL_MS;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Page bridge callbacks (worker thread context, EmvDemo.c role equivalents)
//-----------------------------------------------------------------------------

// Multi-AID selection: returns selected index (>=0) or -1 (EmvSelectApp role)
int EmvPage_SelectApp(AidCandidate_t *pList, int listNum)
{
    int i, n;

    if (pList == NULL || listNum <= 0)
        return -1;

    n = listNum > EMV_PAY_AID_MAX ? EMV_PAY_AID_MAX : listNum;
    for (i = 0; i < n; i++)
    {
        int len;
        emv_pay_aid_t *a = &s_ctx.aidList[i];

        memset(a, 0, sizeof(*a));
        len = pList[i]._aid_len > 16 ? 16 : pList[i]._aid_len;
        if (len > 0)
            Lib_Bcd2Asc(pList[i]._aid, a->aid, len);
        len = pList[i]._lable_len > 23 ? 23 : pList[i]._lable_len;
        if (len > 0)
            memcpy(a->label, pList[i]._lable, len);
    }
    s_ctx.aidCount = n;

    emv_pay_post_req(EMV_PAY_REQ_AID_SELECT);
    if (!emv_pay_wait_answer(EMV_PAY_AID_TIMEOUT_SEC))
        s_ctx.answer = -1;
    emv_pay_end_req();

    Lib_Trace("emv_pay: AID select index = %d", s_ctx.answer);
    return s_ctx.answer;
}

// PIN entry (plaintext via page keypad, EmvInputPin role)
// returns: >0 pin length, 0 bypass, -1 fail/cancel
int EmvPage_InputPin(EMV_INPUT_PIN_TYPE cType, char *pan, char *pszPin)
{
    int ok;
    int ret;

    (void)pan;
    s_ctx.pinType = (int)cType;
    s_ctx.pin[0] = '\0';

    Lib_Trace("emv_pay: InputPin enter type=%d", (int)cType);

    emv_pay_post_req(EMV_PAY_REQ_PIN);
    ok = emv_pay_wait_answer(EMV_PAY_PIN_TIMEOUT_SEC);
    emv_pay_end_req();

    if (!ok)
    {
        Lib_Trace("emv_pay: InputPin cancel/timeout");
        return -1;
    }
    if (s_ctx.pin[0] == '\0')
    {
        Lib_Trace("emv_pay: InputPin bypass");
        return 0;               // bypass
    }
    strcpy(pszPin, s_ctx.pin);
    ret = strlen(s_ctx.pin);
    Lib_Trace("emv_pay: InputPin done len=%d", ret);
    return ret;
}

/* Beep bridge: the worker only registers a request; the LVGL thread plays it throttled via emv_timer_cb.
 * Calling Lib_Beep directly on this thread runs the whole audio player create/play/delete flow,
 * which stalls the audio sub-thread when concurrent with UI-thread beeps (the hang signature of rounds 888/aaa/bbb/ccc). */
void EmvPage_Beep(unsigned long ms)
{
    if (ms == 0)
        ms = 100;
    if (ms > 1000)
        ms = 1000;
    s_ctx.beepMs = ms;
}

// Kernel display prompt (EmvProcessDisp role)
int EmvPage_ProcessDisp(EmvKernelDisp type)
{
    switch (type)
    {
    case EMV_DISP_READING_CARD:
        strcpy(s_ctx.statusText, "Reading card...");
        break;
    case EMV_DISP_SEE_PHONE:
        EmvPage_Beep(300);
        strcpy(s_ctx.statusText, "See phone for instructions");
        break;
    case EMV_DISP_NFC_RETAP:
        EmvPage_Beep(300);
        strcpy(s_ctx.statusText, "Tap card again");
        break;
    case EMV_DISP_REMOVE_CARD:
        EmvPage_Beep(300);
        strcpy(s_ctx.statusText, "Remove card");
        break;
    case EMV_DISP_OFFLINE_PIN_BLOCKED:
        EmvPage_Beep(300);
        strcpy(s_ctx.statusText, "Offline PIN blocked");
        break;
    default:
        break;
    }
    return 0;
}

// Simulated online process (onlineProcess role in EmvDemo.c): approve with "00"
// NOTE: runs in the emvPay worker thread - it must never block. The visible
//       "Processing online..." delay is done by page_emv.c's lv_timer.
int EmvPage_OnlineProcess(EmvOnlineData_t *pstOnlineData)
{
    Lib_Trace("emv_pay: OnlineProcess enter");

    memset(pstOnlineData->iccResponse, 'F', 2);
    strcpy(pstOnlineData->iccResponse, "00");
    strcpy(s_ctx.statusText, "Processing online...");
    s_ctx.onlinePending = 1;

    Lib_Trace("emv_pay: OnlineProcess exit, resp = %s", pstOnlineData->iccResponse);
    return 0;
}

// MSR PIN entry (InputPIN role in EmvDemo.c): page keypad, 4+ digits
// returns: RET_OK / UI_INPUT_CANCEL / UI_INPUT_TIMEOUT
short EmvPage_MsrInputPin(void)
{
    int ok;

    s_ctx.pinType = EMV_ONLINEPIN_INPUT;
    s_ctx.pin[0] = '\0';
    strcpy(s_ctx.statusText, "Enter PIN");

    Lib_Trace("emv_pay: MsrInputPin enter");

    emv_pay_post_req(EMV_PAY_REQ_PIN);
    ok = emv_pay_wait_answer(EMV_PAY_AID_TIMEOUT_SEC);
    Lib_Trace("emv_pay: MsrInputPin wake ok=%d pin_len=%d", ok,
              (int)strlen(s_ctx.pin));
    emv_pay_end_req();

    if (!ok)
    {
        Lib_Trace("emv_pay: MsrInputPin cancel/timeout");
        return UI_INPUT_CANCEL;
    }
    if (s_ctx.pin[0] == '\0')
    {
        Lib_Trace("emv_pay: MsrInputPin empty -> cancel");
        return UI_INPUT_CANCEL;
    }
    Lib_Trace("emv_pay: MsrInputPin done len=%d", (int)strlen(s_ctx.pin));
    return RET_OK;
}

//-----------------------------------------------------------------------------
// Result collection
//-----------------------------------------------------------------------------
static void emv_pay_collect_result(void)
{
    unsigned char *p;
    int len = 0;
    int i, n;
    char asc[48];

    Lib_Trace("emv_pay: collect enter");

    /* PAN: tag 5A, fallback to track2 (57) */
    p = Emv_GetCoreData(0x5A, &len);
    if (p != NULL && len >= 4)
    {
        n = len > 8 ? 8 : len;
        memset(asc, 0, sizeof(asc));
        Lib_Bcd2Asc(p, asc, n);
        for (i = 0; asc[i] >= '0' && asc[i] <= '9'; i++)
            ;
        asc[i] = '\0';
        strncpy(s_ctx.panResult, asc, sizeof(s_ctx.panResult) - 1);
    }
    else
    {
        p = Emv_GetCoreData(0x57, &len);
        if (p != NULL && len >= 4)
        {
            n = len > 20 ? 20 : len;
            memset(asc, 0, sizeof(asc));
            Lib_Bcd2Asc(p, asc, n);
            for (i = 0; asc[i] >= '0' && asc[i] <= '9'; i++)
                ;
            asc[i] = '\0';
            strncpy(s_ctx.panResult, asc, sizeof(s_ctx.panResult) - 1);
        }
    }

    /* expiry: tag 5F24 YYMM -> MM/YY */
    p = Emv_GetCoreData(0x5F24, &len);
    if (p != NULL && len >= 2)
        snprintf(s_ctx.expiryResult, sizeof(s_ctx.expiryResult), "%02X/%02X", p[1], p[0]);

    /* application label: tag 50 */
    p = Emv_GetCoreData(0x50, &len);
    if (p != NULL && len > 0)
    {
        n = len > (int)sizeof(s_ctx.appResult) - 1 ? (int)sizeof(s_ctx.appResult) - 1 : len;
        memcpy(s_ctx.appResult, p, n);
        s_ctx.appResult[n] = '\0';
    }

    /* fallback: PAN captured during confirmation request */
    if (s_ctx.panResult[0] == '\0' && s_ctx.pan[0] != '\0')
        strncpy(s_ctx.panResult, s_ctx.pan, sizeof(s_ctx.panResult) - 1);
}

static const char *msr_card_type_text(unsigned char type)
{
    switch (type)
    {
    case CARD_VISA:       return "VISA";
    case CARD_MASTERCARD: return "MASTERCARD";
    case CARD_AMEX:       return "AMEX";
    case CARD_DINERS:     return "DINERS";
    case CARD_JCB:        return "JCB";
    case CARD_DISCOVER:   return "DISCOVER";
    case CARD_UNIONPAY:   return "UNIONPAY";
    default:              return "MSR";
    }
}

static void emv_pay_collect_msr(const ST_TRANSDATA *pstTransData)
{
    const ST_CARDINFO *ci = &pstTransData->stCardInfo;

    strncpy(s_ctx.panResult, (const char *)ci->aucCardNo, sizeof(s_ctx.panResult) - 1);
    s_ctx.panResult[sizeof(s_ctx.panResult) - 1] = '\0';

    /* aucExpiredDate is YYMM ASCII -> MM/YY */
    if (strlen((const char *)ci->aucExpiredDate) >= 4)
        snprintf(s_ctx.expiryResult, sizeof(s_ctx.expiryResult), "%c%c/%c%c",
                 ci->aucExpiredDate[2], ci->aucExpiredDate[3],
                 ci->aucExpiredDate[0], ci->aucExpiredDate[1]);

    strncpy(s_ctx.appResult, msr_card_type_text(ci->eCardType), sizeof(s_ctx.appResult) - 1);
}

//-----------------------------------------------------------------------------
// Default terminal parameters (emv_app.bin)
// Format (see app_emv.c trans_cbk_fs_*): page 0 = ICS TLV set, each following
// 384-byte page = one AID parameter TLV set, unused bytes padded 0xFF.
// NOTE: demo values only. Real deployments must download proper AID/CAPK data.
//-----------------------------------------------------------------------------
static int emv_put_tlv(unsigned char *buf, int off, unsigned int tag, int len, const unsigned char *val)
{
    if (tag > 0xFFFF)
    {
        buf[off++] = (unsigned char)(tag >> 16);
        buf[off++] = (unsigned char)(tag >> 8);
        buf[off++] = (unsigned char)(tag);
    }
    else if (tag > 0xFF)
    {
        buf[off++] = (unsigned char)(tag >> 8);
        buf[off++] = (unsigned char)(tag);
    }
    else
    {
        buf[off++] = (unsigned char)(tag);
    }
    buf[off++] = (unsigned char)(len);   /* demo params are always < 0x80 */
    memcpy(&buf[off], val, len);
    return off + len;
}

static int emv_file_exist(const char *name)
{
    int fd = Lib_FileOpen((char *)name, LFS_O_RDONLY);
    if (fd >= 0)
    {
        Lib_FileClose(fd);
        return 1;
    }
    return 0;
}

static void emv_pay_default_params(void)
{
    static const unsigned char aucAids[][7] = {
        {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10},  /* Visa */
        {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10},  /* MasterCard */
        {0xA0, 0x00, 0x00, 0x03, 0x33, 0x10, 0x10},  /* UnionPay */
    };
    unsigned char page[384];
    int fd, off, i, n;

    if (emv_file_exist("emv_app.bin"))
        return;

    fd = Lib_FileOpen("emv_app.bin", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (fd < 0)
        return;

    /* page 0: ICS terminal configuration */
    memset(page, 0xFF, sizeof(page));
    off = 0;
    off = emv_put_tlv(page, off, 0x9F35, 1, (const unsigned char *)"\x22");               /* terminal type */
    off = emv_put_tlv(page, off, 0x9F33, 3, (const unsigned char *)"\xE0\xF8\xC8");       /* terminal capabilities */
    off = emv_put_tlv(page, off, 0x9F1A, 2, (const unsigned char *)"\x08\x40");           /* country code 840 */
    off = emv_put_tlv(page, off, 0x5F2A, 2, (const unsigned char *)"\x08\x40");           /* currency code 840 */
    off = emv_put_tlv(page, off, 0x9F15, 2, (const unsigned char *)"\x60\x11");           /* MCC 6011 */
    off = emv_put_tlv(page, off, 0x9F16, 15, (const unsigned char *)"SMARTDEMO0001 ");    /* merchant ID */
    off = emv_put_tlv(page, off, 0xDF8123, 4, (const unsigned char *)"\x00\x00\x00\x00"); /* cless floor limit */
    off = emv_put_tlv(page, off, 0xDF8124, 4, (const unsigned char *)"\x00\x98\x96\x80"); /* cless limit no CDCVM */
    off = emv_put_tlv(page, off, 0xDF8125, 4, (const unsigned char *)"\x00\x98\x96\x80"); /* cless limit CDCVM */
    off = emv_put_tlv(page, off, 0xDF8126, 4, (const unsigned char *)"\x00\x00\x00\x00"); /* CVM required limit */
    Lib_FileWrite(fd, page, sizeof(page));

    /* pages 1..n: one AID parameter set per page */
    n = sizeof(aucAids) / sizeof(aucAids[0]);
    for (i = 0; i < n; i++)
    {
        memset(page, 0xFF, sizeof(page));
        off = 0;
        off = emv_put_tlv(page, off, 0x9F06, 7, aucAids[i]);                              /* AID */
        off = emv_put_tlv(page, off, 0x9F09, 1, (const unsigned char *)"\x01");           /* CA public key index */
        off = emv_put_tlv(page, off, 0x9F35, 1, (const unsigned char *)"\x22");
        off = emv_put_tlv(page, off, 0x9F33, 3, (const unsigned char *)"\xE0\xF8\xC8");
        off = emv_put_tlv(page, off, 0x9F1A, 2, (const unsigned char *)"\x08\x40");
        off = emv_put_tlv(page, off, 0x5F2A, 2, (const unsigned char *)"\x08\x40");
        off = emv_put_tlv(page, off, 0x9F1B, 4, (const unsigned char *)"\x00\x00\x00\x00");   /* floor limit */
        off = emv_put_tlv(page, off, 0x9F8208, 4, (const unsigned char *)"\x00\x98\x96\x80"); /* cless txn limit */
        off = emv_put_tlv(page, off, 0x9F8209, 4, (const unsigned char *)"\x00\x00\x00\x00"); /* cless CVM limit */
        off = emv_put_tlv(page, off, 0x9F820A, 4, (const unsigned char *)"\x00\x00\x00\x00"); /* cless floor limit */
        Lib_FileWrite(fd, page, sizeof(page));
    }

    Lib_FileClose(fd);
    Lib_Trace("emv_pay: default emv_app.bin created (%d AIDs)", n);
}

//-----------------------------------------------------------------------------
// Worker thread: CardDemo() dispatch (EMV kernel or MSR)
//-----------------------------------------------------------------------------
static void emv_pay_worker(void *arg)
{
    ST_TRANSDATA transData;
    EMV_L2_Return emvRet;
    int ret;
    unsigned char stackProbe;

    (void)arg;

    /* Stack watermark base: the local variable address at the worker entry ≈ stack top (highest address) */
    g_emvStackTop = (unsigned long)&stackProbe;
    g_emvStackMin = g_emvStackTop;

    /* 1. fill transaction data (amount comes from payment page input) */
    memset(&transData, 0, sizeof(transData));
    snprintf((char *)transData.trans_amount, sizeof(transData.trans_amount), "%012lu", s_ctx.amountCents);
    transData.trans_type = EMV_L2_SALE;
    transData.cardDetectType = s_cardMode;
    /* trans_time all-zero: EMV_data_init fills current date/time */

    if (s_cardMode == MSC_DETECETED)
        strcpy(s_ctx.statusText, "Reading track data...");
    else
        strcpy(s_ctx.statusText, "Processing...");

    Lib_Trace("emv_pay: worker enter mode=%d amount=%012lu", (int)s_cardMode,
              s_ctx.amountCents);

    /* 2. run the EmvNew flow (EmvDemo registers its own page callbacks) */
    ret = CardDemo(s_cardMode, &transData);

    Lib_Trace("emv_pay: CardDemo ret=%x", ret);

    /* 3. map result */
    if (s_cardMode == MSC_DETECETED)
    {
        if (ret == 0)
            emvRet = APP_RC_COMPLETED;
        else if (ret == UI_INPUT_CANCEL || ret == UI_INPUT_TIMEOUT)
            emvRet = APP_RC_CANCEL;
        else if (ret == 0x8004)
            emvRet = APP_RC_CARD_NOT_SUPPORT;
        else
            emvRet = APP_RC_TERMINAL;

        emv_pay_collect_msr(&transData);
        Lib_Trace("emv_pay: msr collect pan='%s' exp='%s' app='%s'",
                  s_ctx.panResult, s_ctx.expiryResult, s_ctx.appResult);
    }
    else
    {
        emvRet = EmvDemo_GetLastResult();
        emv_pay_collect_result();
    }

    s_ctx.emvRet = emvRet;
    s_ctx.finished = 1;
    s_ctx.state = EMV_PAY_ST_DONE;
    Lib_Trace("emv_pay: finished, mode = %d, ret = %x, emvRet = %d",
              s_cardMode, ret, (int)emvRet);
    Lib_PrintfHeapInfo();

    /* Returning from a thread entry function on OSI jumps to an invalid return address (device hangs without reboot),
     * so the thread must delete itself; this also releases the thread's 128KB stack, otherwise every transaction leaks 128KB. */
    Lib_Trace("emv_pay: thread self delete");
    Lib_osiThreadDelete(Lib_osiThreadCurrent());
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------
int emv_pay_start(unsigned long amountCents, short cardMode)
{
    void *thread;

    if (s_ctx.started && !s_ctx.finished)
        return -1;              // busy

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = EMV_PAY_ST_RUNNING;
    s_ctx.amountCents = amountCents;
    s_cardMode = cardMode;
    s_cancelReq = 0;

    /* make sure terminal AID parameters exist (EMV only) */
    if (cardMode != MSC_DETECETED)
        emv_pay_default_params();

    Lib_Trace("emv_pay: start amount=%lu mode=%d", amountCents, (int)cardMode);
    Lib_PrintfHeapInfo();

    thread = Lib_osiThreadCreate("emvPay", emv_pay_worker, NULL,
                                 OSI_PRIORITY_NORMAL, EMV_PAY_STACK_SIZE, 0);
    if (thread == NULL)
    {
        Lib_Trace("emv_pay: thread create failed");
        s_ctx.finished = 1;
        s_ctx.state = EMV_PAY_ST_DONE;
        s_ctx.emvRet = APP_RC_TERMINAL;
        strcpy(s_ctx.statusText, "Create thread failed");
        return -1;
    }

    Lib_Trace("emv_pay: thread created %lx, stack=%dKB",
              (unsigned long)thread, (int)(EMV_PAY_STACK_SIZE / 1024));
    s_ctx.started = 1;
    return 0;
}

void emv_pay_cancel(void)
{
    Lib_Trace("emv_pay: cancel req");
    s_cancelReq = 1;
}

void emv_pay_answer_aid(int index)
{
    s_ctx.answer = index;
    s_ctx.answerReady = 1;
}

void emv_pay_answer_pin(const char *pin)
{
    int n;

    memset(s_ctx.pin, 0, sizeof(s_ctx.pin));
    if (pin != NULL && pin[0] != '\0')
    {
        n = strlen(pin);
        if (n > (int)sizeof(s_ctx.pin) - 1)
            n = (int)sizeof(s_ctx.pin) - 1;
        memcpy(s_ctx.pin, pin, n);
    }
    s_ctx.answerReady = 1;
    Lib_Trace("emv_pay: answer pin ready, len=%d", (int)strlen(s_ctx.pin));
}

void emv_pay_answer_confirm(int ok)
{
    s_ctx.answer = ok ? 1 : 0;
    s_ctx.answerReady = 1;
}

emv_pay_ctx_t *emv_pay_ctx(void)
{
    return &s_ctx;
}

const char *emv_pay_result_text(EMV_L2_Return ret)
{
    switch (ret)
    {
    case APP_RC_COMPLETED:                     return "Transaction success";
    case APP_RC_TERMINAL:                      return "Terminal error";
    case APP_RC_CANCEL:                        return "Transaction canceled";
    case APP_RC_EMV_DENAIL:
    case APP_RC_EMV_GAC2_DENAIL:               return "Transaction declined";
    case APP_RC_NFC_NOT_ALLOW:                 return "Transaction not allowed";
    case APP_RC_EMV_APP_BLOCK:                 return "Application blocked";
    case APP_RC_EMV_CARD_BLOCK:                return "Card blocked";
    case APP_RC_EMV_APP_SEE_PHONE:             return "See phone for instructions";
    case APP_RC_EMV_TRANS_TRY_ANOTHER_INTERFACE: return "Try another interface";
    case APP_RC_EMV_TRANS_GPO_NOT_SUPPORT:     return "GPO not supported";
    case APP_RC_FALL_BACK:                     return "Fallback to magnetic";
    case APP_RC_CARD_NOT_SUPPORT:              return "Card not supported";
    case APP_RC_NFC_RETAP_TIMEOUT:             return "Retap timeout";
    case APP_RC_NFC_RETAP_CANCEL:              return "Retap canceled";
    case APP_RC_NFC_TERMINAL:                  return "Terminal error";
    case APP_RC_NFC_DOUBLETAP_DENAIL:          return "Second tap declined";
    case APP_RC_NFC_MULTI_CARD:                return "Multiple cards detected";
    case APP_RC_NFC_TRY_AGAIN:                 return "Please try again";
    case APP_RC_TRANS_REVERSEL:                return "Transaction reversed";
    default:                                   return "Transaction failed";
    }
}

//-----------------------------------------------------------------------------
// LvUiWaitKey bridge for app_emv.c nfc_try_again():
// waits for page cancellation up to timeoutSec, returns KB_CANCEL on cancel,
// LVGL_OPER_TIMEOUT on timeout.
//-----------------------------------------------------------------------------
int LvUiWaitKey(int timeoutSec)
{
    int waited = 0;

    while (waited < timeoutSec * 1000)
    {
        if (s_cancelReq)
            return KB_CANCEL;
        Lib_DelayMs(EMV_PAY_POLL_MS);
        waited += EMV_PAY_POLL_MS;
    }
    return LVGL_OPER_TIMEOUT;
}
