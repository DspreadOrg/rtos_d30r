/**********************************************************************************************************
 * File Name     : page_emv.c
 * Description   : EMV / MSR transaction flow page
 *
 *   Pushed by page_payment after a card is detected. All card types go through EmvNew:
 *   - ICC / Contactless: EmvNew EMV L2 kernel (EmvDemo -> Emv_Process)
 *   - MSR: track parsing flow of EmvNew's MccDemo
 *   The page polls emv_pay_ctx via lv_timer and answers AID selection / PIN entry / PAN confirmation,
 *   and shows the PAN, expiry, application and transaction result returned by the kernel.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "emv_pay.h"
#include "LibApi.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define EMV_TICK_MS         100
#define EMV_RESULT_HOLD_TICKS 8     /* Hold result about 800ms after online completion (replaces worker-side delay) */
#define EMV_BTN_Y           580
#define EMV_BTN_H           64
#define EMV_BTN_W           216

#define EMV_PIN_MAX         12

/* Debug switch: investigating "whole unit hangs after pressing OK", disable all Lib_Beep on this page for now.
 * Audio player create/play/delete goes through RPCs such as DM_AudStreamStop / AUD_SetCodecOpStatus,
 * and previous hangs all occurred 0.3~0.4s after the audio flow. */
#define EMV_BEEP_ENABLE     0

#define PAD_CARD_Y          296
#define PAD_CARD_H          400
#define PAD_KEY_X0          20
#define PAD_KEY_Y0          70
#define PAD_KEY_W           136
#define PAD_KEY_H           72
#define PAD_KEY_GAP         8
#define PAD_KEY_COLS        3

static short s_cardMode = SCI_DETECETED;        /* SCI_DETECETED / NFC_DETECETED / MSC_DETECETED */
static unsigned long s_amountCents = 0;

static lv_obj_t *s_amountLabel = NULL;
static lv_obj_t *s_statusLabel = NULL;
static lv_obj_t *s_cardLabel = NULL;
static lv_obj_t *s_expiryLabel = NULL;
static lv_obj_t *s_appLabel = NULL;
static lv_obj_t *s_stepsCard = NULL;
static lv_obj_t *s_keypadCard = NULL;
static lv_obj_t *s_pinLabel = NULL;
static lv_obj_t *s_keypadHint = NULL;
static lv_obj_t *s_printButton = NULL;
static lv_obj_t *s_doneButton = NULL;
static lv_timer_t *s_timer = NULL;

/* Interactive widgets (reuse the flow card area) */
static lv_obj_t *s_aidHint = NULL;
static lv_obj_t *s_aidButton[EMV_PAY_AID_MAX];
static lv_obj_t *s_confirmPanLabel = NULL;
static lv_obj_t *s_confirmButton = NULL;
static lv_obj_t *s_rejectButton = NULL;
static lv_obj_t *s_cancelButton = NULL;
static lv_obj_t *s_resultLabel = NULL;

static char s_pinBuf[EMV_PIN_MAX + 1];
static bool s_resultShown = false;
static int s_resultHoldTicks = 0;

static void emv_show_obj(lv_obj_t *obj, bool show)
{
    if (obj == NULL)
    {
        return;
    }

    if (show)
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_color_t emv_status_color(void)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();

    if (ctx->state == EMV_PAY_ST_DONE)
    {
        return (ctx->emvRet == APP_RC_COMPLETED) ? APP_COLOR_SUCCESS : APP_COLOR_WARN;
    }

    return (ctx->req == EMV_PAY_REQ_PIN) ? APP_COLOR_ACCENT : APP_COLOR_SUB;
}

static void emv_beep(unsigned long ms)
{
#if EMV_BEEP_ENABLE
    Lib_Beep(ms);
#else
    (void)ms;
#endif
}

static void emv_pin_display(void)
{
    char text[48] = "PIN ";
    size_t len = strlen(s_pinBuf);
    int i;

    for (i = 0; i < EMV_PIN_MAX; i++)
    {
        strcat(text, (i < (int)len) ? " *" : " -");
    }

    if (s_pinLabel != NULL)
    {
        lv_label_set_text(s_pinLabel, text);
    }
}

static void emv_show_aid(bool show)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();
    int i;

    emv_show_obj(s_aidHint, show);

    for (i = 0; i < EMV_PAY_AID_MAX; i++)
    {
        if (s_aidButton[i] == NULL)
        {
            continue;
        }

        if (show && i < ctx->aidCount)
        {
            lv_obj_t *label = lv_obj_get_child(s_aidButton[i], 0);
            char text[48];

            if (ctx->aidList[i].label[0] != '\0')
            {
                snprintf(text, sizeof(text), "%d. %s", i + 1, ctx->aidList[i].label);
            }
            else
            {
                snprintf(text, sizeof(text), "%d. AID %s", i + 1, ctx->aidList[i].aid);
            }

            if (label != NULL)
            {
                lv_label_set_text(label, text);
            }

            emv_show_obj(s_aidButton[i], true);
        }
        else
        {
            emv_show_obj(s_aidButton[i], false);
        }
    }
}

static void emv_refresh(void)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();
    char text[96];

    if (s_amountLabel != NULL)
    {
        snprintf(text, sizeof(text), "%lu.%02lu",
                 ctx->amountCents / 100, ctx->amountCents % 100);
        lv_label_set_text(s_amountLabel, text);
    }

    /* Hold display during online processing: kernel callback no longer blocks, delay handled by this page timer */
    if (ctx->state == EMV_PAY_ST_DONE && ctx->onlinePending && !s_resultShown)
    {
        if (s_resultHoldTicks < EMV_RESULT_HOLD_TICKS)
        {
            s_resultHoldTicks++;
            if (s_statusLabel != NULL)
            {
                lv_label_set_text(s_statusLabel, LIB_TR("Processing online..."));
                lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_SUB, 0);
            }
            emv_show_obj(s_cancelButton, false);
            return;
        }
    }

    if (s_cardLabel != NULL)
    {
        if (ctx->panResult[0] != '\0')
        {
            lv_label_set_text(s_cardLabel, ctx->panResult);
        }
        else if (ctx->pan[0] != '\0')
        {
            lv_label_set_text(s_cardLabel, ctx->pan);
        }
    }

    if (s_expiryLabel != NULL && ctx->expiryResult[0] != '\0')
    {
        lv_label_set_text(s_expiryLabel, ctx->expiryResult);
    }

    if (s_appLabel != NULL && ctx->appResult[0] != '\0')
    {
        lv_label_set_text(s_appLabel, ctx->appResult);
    }

    if (s_statusLabel != NULL)
    {
        if (ctx->state == EMV_PAY_ST_DONE)
        {
            snprintf(text, sizeof(text), "%s", emv_pay_result_text(ctx->emvRet));
        }
        else
        {
            snprintf(text, sizeof(text), "%s",
                     ctx->statusText[0] != '\0' ? ctx->statusText : "Processing...");
        }

        lv_label_set_text(s_statusLabel, LIB_TR(text));
        lv_obj_set_style_text_color(s_statusLabel, emv_status_color(), 0);
    }

    if (ctx->state == EMV_PAY_ST_DONE)
    {
        emv_show_obj(s_keypadCard, false);
        emv_show_aid(false);
        emv_show_obj(s_confirmPanLabel, false);
        emv_show_obj(s_confirmButton, false);
        emv_show_obj(s_rejectButton, false);

        if (s_resultLabel != NULL)
        {
            lv_label_set_text(s_resultLabel, LIB_TR(emv_pay_result_text(ctx->emvRet)));
            lv_obj_set_style_text_color(s_resultLabel, emv_status_color(), 0);
        }
        emv_show_obj(s_resultLabel, true);

        emv_show_obj(s_cancelButton, false);
        emv_show_obj(s_printButton, true);
        emv_show_obj(s_doneButton, true);

        if (!s_resultShown)
        {
            s_resultShown = true;
            Lib_Trace("page_emv: result ret=%d pan='%s' exp='%s' app='%s'",
                      (int)ctx->emvRet, ctx->panResult, ctx->expiryResult,
                      ctx->appResult);
            emv_beep(BEEP_TIME_DEF);
            Lib_Trace("page_emv: result beep done");
        }

        return;
    }

    /* Running */
    emv_show_obj(s_printButton, false);
    emv_show_obj(s_doneButton, false);
    emv_show_obj(s_cancelButton, true);
    emv_show_obj(s_resultLabel, false);

    if (ctx->req == EMV_PAY_REQ_PIN)
    {
        /* PIN keypad fills the flow card area, the bottom Cancel would cover the last keypad row, so hide it */
        emv_show_obj(s_cancelButton, false);

        if (s_keypadHint != NULL)
        {
            if (ctx->pinType == EMV_LAST_OFFLINEPIN_INPUT)
            {
                lv_label_set_text(s_keypadHint, LIB_TR("OFFLINE PIN (last try), 4-12 digits"));
            }
            else if (ctx->pinType == EMV_OFFLINEPIN_INPUT)
            {
                lv_label_set_text(s_keypadHint, LIB_TR("OFFLINE PIN, 4-12 digits, empty OK = bypass"));
            }
            else if (s_cardMode == MSC_DETECETED)
            {
                lv_label_set_text(s_keypadHint, LIB_TR("MSR PIN, 4-12 digits"));
            }
            else
            {
                lv_label_set_text(s_keypadHint, LIB_TR("ONLINE PIN, 4-12 digits, empty OK = bypass"));
            }
        }

        emv_show_obj(s_stepsCard, false);
        emv_show_obj(s_keypadCard, true);
        emv_pin_display();
        return;
    }

    emv_show_obj(s_keypadCard, false);
    emv_show_obj(s_stepsCard, true);

    if (ctx->req == EMV_PAY_REQ_AID_SELECT)
    {
        emv_show_obj(s_confirmPanLabel, false);
        emv_show_obj(s_confirmButton, false);
        emv_show_obj(s_rejectButton, false);
        emv_show_aid(true);
    }
    else if (ctx->req == EMV_PAY_REQ_CARD_CONFIRM)
    {
        emv_show_aid(false);

        if (s_confirmPanLabel != NULL)
        {
            lv_label_set_text(s_confirmPanLabel, ctx->pan);
        }

        emv_show_obj(s_confirmPanLabel, true);
        emv_show_obj(s_confirmButton, true);
        emv_show_obj(s_rejectButton, true);
    }
    else
    {
        emv_show_aid(false);
        emv_show_obj(s_confirmPanLabel, false);
        emv_show_obj(s_confirmButton, false);
        emv_show_obj(s_rejectButton, false);
    }
}

/* ---------------- In-page PIN keypad ---------------- */

static void emv_pin_submit(void)
{
    size_t len = strlen(s_pinBuf);

    Lib_Trace("page_emv: pin submit len=%d", (int)len);

    if (len != 0 && len < 4)
    {
        Lib_Trace("page_emv: pin rejected (len=%d)", (int)len);
        if (s_statusLabel != NULL)
        {
            lv_label_set_text(s_statusLabel, LIB_TR("Enter 4-12 digits or leave empty"));
            lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_WARN, 0);
        }
        return;
    }

    emv_pay_answer_pin(s_pinBuf);
    Lib_Trace("page_emv: pin answered");
    s_pinBuf[0] = '\0';
}

/* value: 0~9 digit, -1 clear, -2 confirm */
static void emv_pin_input(int value)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();
    size_t len;

    if (ctx->req != EMV_PAY_REQ_PIN)
    {
        return;
    }

    if (value >= 0 && value <= 9)
    {
        len = strlen(s_pinBuf);
        if ((int)len < EMV_PIN_MAX)
        {
            s_pinBuf[len] = (char)('0' + value);
            s_pinBuf[len + 1] = '\0';
        }
        emv_pin_display();
        return;
    }

    if (value == -1)
    {
        s_pinBuf[0] = '\0';
        emv_pin_display();
        return;
    }

    if (value == -2)
    {
        emv_pin_submit();
    }
}

static void emv_pin_key_cb(lv_event_t *e)
{
    int value = (int)(intptr_t)lv_event_get_user_data(e);

    emv_pin_input(value);
}

/* ---------------- Interaction callbacks ---------------- */

static void emv_aid_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    Lib_Trace("page_emv: aid select idx=%d", index);
    emv_pay_answer_aid(index);
}

static void emv_confirm_cb(lv_event_t *e)
{
    (void)e;
    Lib_Trace("page_emv: confirm pan='%s'", emv_pay_ctx()->pan);
    emv_pay_answer_confirm(1);
}

static void emv_reject_cb(lv_event_t *e)
{
    (void)e;
    Lib_Trace("page_emv: reject pan");
    emv_pay_answer_confirm(0);
}

static void emv_cancel_cb(lv_event_t *e)
{
    (void)e;
    Lib_Trace("page_emv: cancel button");
    emv_pay_cancel();
}

/* ---------------- Flow progression ---------------- */

static void emv_timer_cb(lv_timer_t *timer)
{
    static int tick = 0;
    static int beepWait = 0;
    static int lastState = -1;
    static int lastReq = -1;
    static int lastFin = -1;
    emv_pay_ctx_t *ctx = emv_pay_ctx();

    (void)timer;

    /* Print state transitions only on change to avoid log flooding (the last transition shows where a hang occurred) */
    if (ctx->state != lastState || ctx->req != lastReq || ctx->finished != lastFin)
    {
        Lib_Trace("page_emv: tick state=%d req=%d fin=%d ret=%d pan='%s'",
                  ctx->state, ctx->req, ctx->finished, (int)ctx->emvRet,
                  ctx->panResult);
        lastState = ctx->state;
        lastReq = ctx->req;
        lastFin = ctx->finished;
    }

    emv_refresh();

    /* Beeps are played on this thread only: the worker just records ctx->beepMs.
     * Minimum interval 8×100ms to avoid repeated create/play/delete of the audio player. */
    if (beepWait > 0)
        beepWait--;
    if (ctx->beepMs != 0 && beepWait == 0)
    {
        unsigned long ms = ctx->beepMs;
        ctx->beepMs = 0;
        beepWait = 8;
        Lib_Trace("page_emv: beep req ms=%lu (enable=%d)", ms, EMV_BEEP_ENABLE);
        emv_beep(ms);
        Lib_Trace("page_emv: beep done");
    }

    /* Heartbeat: used to tell whether the LVGL main thread is still alive when the EMV worker hangs
       (log stopping alone cannot distinguish "only worker hung" from "whole unit hung") */
    if (ctx->started && !ctx->finished && (++tick % 2) == 0)
        Lib_Trace("page_emv: alive, state=%d, req=%d", ctx->state, ctx->req);
}

/* ---------------- Printing ---------------- */

static int emv_print_receipt(void)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();
    const char *resultText = (ctx->emvRet == APP_RC_COMPLETED) ? "APPROVED" : "DECLINED";
    const char *modeText = (s_cardMode == NFC_DETECETED) ? "CONTACTLESS"
                         : (s_cardMode == MSC_DETECETED) ? "MSR" : "CHIP";
    char amount[24] = { 0 };
    char line[80] = { 0 };
    unsigned char bcd[7] = { 0 };
    char timeStr[13] = { 0 };
    int rc;

    rc = Lib_PrnOpen();
    if (rc != RET_OK)
    {
        return rc;
    }

    Lib_PrnSetSpace(8);
    Lib_PrnSetFont(FONT32, 1, 1);
    Lib_PrnStrEx(ALIGN_M, (char *)"EMV RECEIPT", 11);

    Lib_PrnSetFont(FONT24, 1, 1);
    Lib_PrnStrEx(ALIGN_M, (char *)"----------------------", 22);

    snprintf(line, sizeof(line), "APP    : %s",
             ctx->appResult[0] != '\0' ? ctx->appResult : "-");
    Lib_PrnStrEx(ALIGN_L, (char *)line, (unsigned long)strlen(line));

    snprintf(line, sizeof(line), "CARD   : %s",
             ctx->panResult[0] != '\0' ? ctx->panResult : "-");
    Lib_PrnStrEx(ALIGN_L, (char *)line, (unsigned long)strlen(line));

    snprintf(line, sizeof(line), "EXP    : %s",
             ctx->expiryResult[0] != '\0' ? ctx->expiryResult : "-");
    Lib_PrnStrEx(ALIGN_L, (char *)line, (unsigned long)strlen(line));

    snprintf(line, sizeof(line), "MODE   : %s", modeText);
    Lib_PrnStrEx(ALIGN_L, (char *)line, (unsigned long)strlen(line));

    snprintf(amount, sizeof(amount), "%lu.%02lu",
             ctx->amountCents / 100, ctx->amountCents % 100);
    snprintf(line, sizeof(line), "AMOUNT : %s", amount);
    Lib_PrnStrEx(ALIGN_L, (char *)line, (unsigned long)strlen(line));

    Lib_GetDateTime(bcd);
    Lib_Bcd2Asc(bcd, timeStr, 6);
    snprintf(line, sizeof(line), "TIME   : 20%c%c-%c%c-%c%c %c%c:%c%c",
             timeStr[0], timeStr[1], timeStr[2], timeStr[3],
             timeStr[4], timeStr[5], timeStr[6], timeStr[7], timeStr[8], timeStr[9]);
    Lib_PrnStrEx(ALIGN_L, (char *)line, (unsigned long)strlen(line));

    Lib_PrnStrEx(ALIGN_M, (char *)"----------------------", 22);
    Lib_PrnStrEx(ALIGN_M, (char *)resultText, (unsigned long)strlen(resultText));

    Lib_PrnPaperForward(3000);
    Lib_PrnPaperForward(3000);

    rc = Lib_PrnClose();
    if (rc == RET_ERR_TPH_NO_PAPER && Lib_PrnCheckStatus() == 0)
    {
        rc = RET_OK;
    }

    return rc;
}

static void emv_print_cb(lv_event_t *e)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();
    int rc;

    (void)e;

    if (ctx->state != EMV_PAY_ST_DONE)
    {
        Lib_Trace("page_emv: print pressed but state=%d", ctx->state);
        return;
    }

    Lib_Trace("page_emv: print start");

    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR("Printing..."));
        lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_ACCENT, 0);
    }
    lv_refr_now(NULL);

    rc = emv_print_receipt();

    Lib_Trace("page_emv: print done rc=%d", rc);

    if (s_statusLabel != NULL)
    {
        if (rc == RET_OK)
        {
            lv_label_set_text(s_statusLabel, LIB_TR("Print OK"));
            lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_SUCCESS, 0);
        }
        else if (rc == RET_ERR_TPH_NO_PAPER)
        {
            lv_label_set_text(s_statusLabel, LIB_TR("Out of paper"));
            lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_WARN, 0);
        }
        else
        {
            lv_label_set_text(s_statusLabel, LIB_TR("Print failed"));
            lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_WARN, 0);
        }
    }
}

static void emv_done_cb(lv_event_t *e)
{
    (void)e;

    Lib_Trace("page_emv: done button");
    app_page_pop();
}

/* ---------------- Page construction ---------------- */

static lv_obj_t *emv_info_row(lv_obj_t *parent, lv_coord_t y, const char *name)
{
    lv_obj_t *nameLabel = lv_label_create(parent);
    lv_obj_t *valueLabel = lv_label_create(parent);

    lv_label_set_text(nameLabel, LIB_TR(name));
    lv_obj_set_style_text_color(nameLabel, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(nameLabel, app_font(16), 0);
    lv_obj_set_pos(nameLabel, 20, y);

    lv_label_set_text(valueLabel, "-");
    lv_obj_set_width(valueLabel, 260);
    lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(valueLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(valueLabel, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(valueLabel, app_font(18), 0);
    lv_obj_align(valueLabel, LV_ALIGN_TOP_RIGHT, -20, y);

    return valueLabel;
}

static lv_obj_t *emv_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, const char *text, lv_color_t bg)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, LIB_TR(text));
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_center(label);

    return btn;
}

static lv_obj_t *emv_key_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                const char *text, lv_color_t bg, int value, uint32_t font)
{
    lv_obj_t *btn = emv_button(parent, x, y, PAD_KEY_W, PAD_KEY_H, text, bg);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    label = lv_obj_get_child(btn, 0);
    if (label != NULL)
    {
        lv_obj_set_style_text_font(label, app_font(font), 0);
    }

    lv_obj_add_event_cb(btn, emv_pin_key_cb, LV_EVENT_CLICKED, (void *)(intptr_t)value);

    return btn;
}

static void emv_build_keypad(lv_obj_t *parent)
{
    static const char *keys[12] =
    {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "Clear", "0", "OK",
    };
    int i;

    s_pinLabel = lv_label_create(parent);
    lv_label_set_text(s_pinLabel, LIB_TR("PIN  - - - -"));
    lv_obj_set_style_text_color(s_pinLabel, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_pinLabel, app_font(28), 0);
    lv_obj_align(s_pinLabel, LV_ALIGN_TOP_MID, 0, 8);

    s_keypadHint = lv_label_create(parent);
    lv_label_set_text(s_keypadHint, LIB_TR("4-12 digits"));
    lv_obj_set_style_text_color(s_keypadHint, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(s_keypadHint, app_font(14), 0);
    lv_obj_align(s_keypadHint, LV_ALIGN_TOP_MID, 0, 44);

    for (i = 0; i < 12; i++)
    {
        lv_coord_t x = PAD_KEY_X0 + (lv_coord_t)(i % PAD_KEY_COLS) * (PAD_KEY_W + PAD_KEY_GAP);
        lv_coord_t y = PAD_KEY_Y0 + (lv_coord_t)(i / PAD_KEY_COLS) * (PAD_KEY_H + PAD_KEY_GAP);
        int value = -3;
        lv_color_t bg = APP_COLOR_ACCENT;
        uint32_t font = 28;

        if (i < 9)
        {
            value = i + 1;
        }
        else if (i == 9)
        {
            value = -1;
            bg = APP_COLOR_WARN;
            font = 20;
        }
        else if (i == 10)
        {
            value = 0;
        }
        else
        {
            value = -2;
            bg = APP_COLOR_SUCCESS;
            font = 20;
        }

        emv_key_button(parent, x, y, keys[i], bg, value, font);
    }
}

/* Create interactive widgets inside the flow card (all hidden by default) */
static void emv_build_widgets(lv_obj_t *parent)
{
    int i;

    s_aidHint = lv_label_create(parent);
    lv_label_set_text(s_aidHint, LIB_TR("SELECT APPLICATION"));
    lv_obj_set_style_text_color(s_aidHint, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_aidHint, app_font(18), 0);
    lv_obj_set_pos(s_aidHint, 20, 12);

    for (i = 0; i < EMV_PAY_AID_MAX; i++)
    {
        s_aidButton[i] = emv_button(parent, 20, 44 + (lv_coord_t)i * 42,
                                    APP_ROW_W - 40, 36, "", APP_COLOR_ACCENT);
        if (s_aidButton[i] != NULL)
        {
            lv_obj_t *label = lv_obj_get_child(s_aidButton[i], 0);

            if (label != NULL)
            {
                lv_obj_set_style_text_font(label, app_font(18), 0);
            }
            lv_obj_add_event_cb(s_aidButton[i], emv_aid_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);
            lv_obj_add_flag(s_aidButton[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_confirmPanLabel = lv_label_create(parent);
    lv_label_set_text(s_confirmPanLabel, "");
    lv_obj_set_style_text_color(s_confirmPanLabel, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_confirmPanLabel, app_font(24), 0);
    lv_obj_set_width(s_confirmPanLabel, APP_ROW_W - 40);
    lv_label_set_long_mode(s_confirmPanLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_confirmPanLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_confirmPanLabel, LV_ALIGN_TOP_MID, 0, 52);

    s_confirmButton = emv_button(parent, 20, 110, (APP_ROW_W - 48) / 2, 48,
                                 "Confirm", APP_COLOR_SUCCESS);
    if (s_confirmButton != NULL)
    {
        lv_obj_add_event_cb(s_confirmButton, emv_confirm_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_confirmButton, LV_OBJ_FLAG_HIDDEN);
    }

    s_rejectButton = emv_button(parent, 28 + (APP_ROW_W - 48) / 2, 110,
                                (APP_ROW_W - 48) / 2, 48, "Cancel", APP_COLOR_WARN);
    if (s_rejectButton != NULL)
    {
        lv_obj_add_event_cb(s_rejectButton, emv_reject_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_rejectButton, LV_OBJ_FLAG_HIDDEN);
    }

    s_resultLabel = lv_label_create(parent);
    lv_label_set_text(s_resultLabel, "");
    lv_obj_set_style_text_color(s_resultLabel, APP_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(s_resultLabel, app_font(24), 0);
    lv_obj_set_width(s_resultLabel, APP_ROW_W - 40);
    lv_label_set_long_mode(s_resultLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_resultLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_resultLabel, LV_ALIGN_TOP_MID, 0, 80);

    emv_show_obj(s_aidHint, false);
    emv_show_obj(s_resultLabel, false);
}

static void emv_create(app_page_t *page)
{
    lv_obj_t *card = NULL;
    lv_obj_t *label = NULL;

    s_pinBuf[0] = '\0';
    s_resultShown = false;
    s_resultHoldTicks = 0;

    /* Amount and current step hint */
    card = app_card(page->body, APP_ROW_X, 12, APP_ROW_W, 108);
    if (card != NULL)
    {
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

        s_amountLabel = lv_label_create(card);
        lv_label_set_text(s_amountLabel, "0.00");
        lv_obj_set_style_text_color(s_amountLabel, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_amountLabel, app_font(32), 0);
        lv_obj_align(s_amountLabel, LV_ALIGN_TOP_MID, 0, 10);

        s_statusLabel = lv_label_create(card);
        lv_label_set_text(s_statusLabel, LIB_TR("Reading card..."));
        lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_SUB, 0);
        lv_obj_set_style_text_font(s_statusLabel, app_font(16), 0);
        lv_obj_align(s_statusLabel, LV_ALIGN_BOTTOM_MID, 0, -12);
    }

    /* Card information */
    card = app_card(page->body, APP_ROW_X, 132, APP_ROW_W, 152);
    if (card != NULL)
    {
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

        s_cardLabel = emv_info_row(card, 16, "CARD");
        s_expiryLabel = emv_info_row(card, 62, "EXPIRY");
        s_appLabel = emv_info_row(card, 108, "APPLICATION");
    }

    /* Flow card / interaction area */
    s_stepsCard = app_card(page->body, APP_ROW_X, PAD_CARD_Y, APP_ROW_W, 248);
    if (s_stepsCard != NULL)
    {
        lv_obj_clear_flag(s_stepsCard, LV_OBJ_FLAG_CLICKABLE);

        label = lv_label_create(s_stepsCard);
        lv_label_set_text(label, LIB_TR("EMV KERNEL"));
        lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(label, app_font(18), 0);
        lv_obj_set_pos(label, 20, 12);

        emv_build_widgets(s_stepsCard);
    }

    /* Built-in large keypad (shown during PIN entry, replaces the flow card) */
    s_keypadCard = app_card(page->body, APP_ROW_X, PAD_CARD_Y, APP_ROW_W, PAD_CARD_H);
    if (s_keypadCard != NULL)
    {
        lv_obj_clear_flag(s_keypadCard, LV_OBJ_FLAG_CLICKABLE);
        emv_build_keypad(s_keypadCard);
        emv_show_obj(s_keypadCard, false);
    }

    /* Bottom buttons: left is Cancel while running, becomes Print after completion */
    s_printButton = emv_button(page->body, APP_ROW_X, EMV_BTN_Y, EMV_BTN_W,
                               EMV_BTN_H, "Print", APP_COLOR_SUCCESS);
    if (s_printButton != NULL)
    {
        lv_obj_add_event_cb(s_printButton, emv_print_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_printButton, LV_OBJ_FLAG_HIDDEN);
    }

    s_cancelButton = emv_button(page->body, APP_ROW_X, EMV_BTN_Y, EMV_BTN_W,
                                EMV_BTN_H, "Cancel", APP_COLOR_WARN);
    if (s_cancelButton != NULL)
    {
        lv_obj_add_event_cb(s_cancelButton, emv_cancel_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_cancelButton, LV_OBJ_FLAG_HIDDEN);
    }

    s_doneButton = emv_button(page->body, APP_ROW_X + EMV_BTN_W + 16, EMV_BTN_Y,
                              EMV_BTN_W, EMV_BTN_H, "Done", APP_COLOR_ACCENT);
    if (s_doneButton != NULL)
    {
        lv_obj_add_event_cb(s_doneButton, emv_done_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(s_doneButton, LV_OBJ_FLAG_HIDDEN);
    }

    s_timer = lv_timer_create(emv_timer_cb, EMV_TICK_MS, NULL);

    Lib_Trace("page_emv: create amount=%lu mode=%d timer=%lx",
              s_amountCents, (int)s_cardMode, (unsigned long)s_timer);

    /* Start the EmvNew transaction flow (ICC/contactless kernel or MSR track parsing) */
    {
        int rc = emv_pay_start(s_amountCents, s_cardMode);

        Lib_Trace("page_emv: emv_pay_start rc=%d", rc);

        if (rc != 0)
        {
            if (s_statusLabel != NULL)
            {
                lv_label_set_text(s_statusLabel, LIB_TR("Start EMV failed"));
                lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_WARN, 0);
            }
        }
    }

    emv_refresh();
}

static void emv_destroy(app_page_t *page)
{
    int i;

    (void)page;

    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    {
        emv_pay_ctx_t *ctx = emv_pay_ctx();

        Lib_Trace("page_emv: destroy fin=%d state=%d req=%d", ctx->finished,
                  ctx->state, ctx->req);

        if (!ctx->finished)
        {
            emv_pay_cancel();
        }
    }

    s_pinBuf[0] = '\0';
    s_amountLabel = NULL;
    s_statusLabel = NULL;
    s_cardLabel = NULL;
    s_expiryLabel = NULL;
    s_appLabel = NULL;
    s_stepsCard = NULL;
    s_keypadCard = NULL;
    s_pinLabel = NULL;
    s_keypadHint = NULL;
    s_printButton = NULL;
    s_doneButton = NULL;
    s_cancelButton = NULL;
    s_aidHint = NULL;
    s_confirmPanLabel = NULL;
    s_confirmButton = NULL;
    s_rejectButton = NULL;
    s_resultLabel = NULL;

    for (i = 0; i < EMV_PAY_AID_MAX; i++)
    {
        s_aidButton[i] = NULL;
    }
}

static bool emv_event(app_page_t *page, const app_event_t *ev)
{
    emv_pay_ctx_t *ctx = emv_pay_ctx();

    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    Lib_Trace("page_emv: key=%d state=%d req=%d", (int)ev->param, ctx->state,
              ctx->req);

    if (ctx->state != EMV_PAY_ST_DONE && ev->param == KB_CANCEL)
    {
        emv_pay_cancel();
        return true;
    }

    if (ctx->req == EMV_PAY_REQ_PIN)
    {
        if (ev->param >= KB_DIGIT_ZERO && ev->param <= KB_DIGIT_NINE)
        {
            emv_pin_input((int)(ev->param - KB_DIGIT_ZERO));
            return true;
        }

        if (ev->param == KB_CLEAR)
        {
            emv_pin_input(-1);
            return true;
        }

        if (ev->param == KB_ENTER)
        {
            emv_pin_submit();
            return true;
        }

        return false;
    }

    if (ctx->state == EMV_PAY_ST_DONE && ev->param == KB_ENTER)
    {
        app_page_pop();
        return true;
    }

    return false;
}

static app_page_t s_emvPage =
{
    .id         = "emv",
    .title      = "EMV PAYMENT",
    .on_create  = emv_create,
    .on_event   = emv_event,
    .on_destroy = emv_destroy,
    .user_data  = NULL,
};

app_page_t *page_emv_open(unsigned long amountCents, const char *cardType, const char *cardData)
{
    (void)cardData;

    s_amountCents = amountCents;
    s_cardMode = (cardType != NULL && strcmp(cardType, "Contactless") == 0) ? NFC_DETECETED
               : (cardType != NULL && strcmp(cardType, "MSR") == 0) ? MSC_DETECETED
               : SCI_DETECETED;

    return &s_emvPage;
}
