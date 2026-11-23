//-----------------------------------------------------------------------------
// @file    : emv_pay.h
// @brief   : EMV / MSR payment flow (port of EmvNew/EmvDemo.c entry logic)
// @details : Runs CardDemo()/EmvDemo() (Dspread EMV L2 kernel, EmvNew/app_emv.c
//            + EmvNew/lib) in a worker thread, bridging kernel callbacks
//            (AID select / PIN input / display / online) to the LVGL payment
//            page (App/Source/page_emv.c) via a polled shared context.
//-----------------------------------------------------------------------------
#ifndef __EMV_PAY_H__
#define __EMV_PAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "EmvDemo.h"    /* LibApi + app_emv: EMV_L2_Return / EmvCallBack_t / ST_TRANSDATA */

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Interaction requests the page must answer
typedef enum
{
    EMV_PAY_REQ_NONE = 0,
    EMV_PAY_REQ_AID_SELECT,      // application list selection
    EMV_PAY_REQ_PIN,             // PIN entry (plaintext, demo)
    EMV_PAY_REQ_CARD_CONFIRM,    // card number confirmation
} emv_pay_req_t;

// Flow state
typedef enum
{
    EMV_PAY_ST_IDLE = 0,
    EMV_PAY_ST_RUNNING,          // kernel executing
    EMV_PAY_ST_WAIT_PAGE,        // waiting for the page to answer a request
    EMV_PAY_ST_DONE,             // finished, check emvRet
} emv_pay_state_t;

#define EMV_PAY_AID_MAX  10

// Page-safe copy of an AID candidate (kernel memory only valid during callback)
typedef struct
{
    char aid[36];       // AID hex string
    char label[24];     // application label
} emv_pay_aid_t;

// Shared context: worker writes state, page writes answers
typedef struct
{
    // flow state (worker -> page)
    volatile int            started;
    volatile int            finished;
    volatile emv_pay_state_t state;
    volatile EMV_L2_Return  emvRet;         // final kernel result
    volatile int            onlinePending;  // 1 = online processing done, page keeps prompt
    char                    statusText[64]; // kernel display prompt
    /* Beep request (worker -> page): the worker does not call Lib_Beep directly;
     * the LVGL thread plays it throttled, avoiding repeated cross-thread create/delete of the audio player */
    volatile unsigned long  beepMs;

    // pending request (worker -> page)
    volatile emv_pay_req_t  req;
    emv_pay_aid_t           aidList[EMV_PAY_AID_MAX];
    volatile int            aidCount;
    volatile int            pinType;        // EMV_INPUT_PIN_TYPE
    char                    pan[24];        // PAN for confirmation request

    // answer (page -> worker)
    volatile int            answer;         // AID index / confirm result (1/0)
    char                    pin[16];        // plaintext PIN, empty = bypass
    volatile int            answerReady;

    // transaction result info
    unsigned long           amountCents;
    char                    panResult[24];
    char                    expiryResult[8]; // MM/YY
    char                    appResult[32];
} emv_pay_ctx_t;

//-----------------------------------------------------------------------------
// Page bridge callbacks (registered by EmvDemo.c Initialize_EMV_CallBackFun)
//-----------------------------------------------------------------------------
int   EmvPage_SelectApp(AidCandidate_t *pList, int listNum);
int   EmvPage_InputPin(EMV_INPUT_PIN_TYPE cType, char *pan, char *pszPin);
int   EmvPage_ProcessDisp(EmvKernelDisp type);
/* Request a beep (callable from any thread; only sets a flag, never touches the audio subsystem) */
void  EmvPage_Beep(unsigned long ms);
int   EmvPage_OnlineProcess(EmvOnlineData_t *pstOnlineData);
short EmvPage_MsrInputPin(void);

//-----------------------------------------------------------------------------
// EmvNew/EmvDemo.c entry points used by the bridge
//-----------------------------------------------------------------------------
short CardDemo(short cardMode, ST_TRANSDATA *pstTransData);
EMV_L2_Return EmvDemo_GetLastResult(void);

// Copy magnetic tracks into EmvDemo.c (called by page_payment before starting).
void EmvDemo_SetMsrTracks(const ST_MSC_TRACK *t1, const ST_MSC_TRACK *t2, const ST_MSC_TRACK *t3);

//-----------------------------------------------------------------------------
// API (called from the LVGL page thread)
//-----------------------------------------------------------------------------

// Start one transaction. cardMode: SCI_DETECETED / NFC_DETECETED / MSC_DETECETED.
// Returns 0 on success (worker started), -1 on busy/failure.
int  emv_pay_start(unsigned long amountCents, short cardMode);

// Request cancellation of the running transaction.
void emv_pay_cancel(void);

// Answer the pending request (only valid while req != EMV_PAY_REQ_NONE).
void emv_pay_answer_aid(int index);          // -1 = cancel selection
void emv_pay_answer_pin(const char *pin);    // NULL or "" = bypass
void emv_pay_answer_confirm(int ok);         // 1 = confirm, 0 = reject

// Human readable result text.
const char *emv_pay_result_text(EMV_L2_Return ret);

// Shared context for page polling.
emv_pay_ctx_t *emv_pay_ctx(void);

#ifdef __cplusplus
}
#endif

#endif // __EMV_PAY_H__
