/**********************************************************************************************************
 * File Name     : page_payment.c
 * Description   : Payment page (local demo flow)
 *
 *   Flow: enter amount -> wait for swipe/insert/tap -> go to the EMV flow page (page_emv) once a card is detected.
 *   Card reading is polled with lv_timer without blocking the LVGL thread; readers are closed when leaving the page.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibApi.h"
#include "LibCards.h"
#include "emv_pay.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PAY_MAX_CENTS           99999999UL
#define PAY_POLL_MS             150
#define PAY_MSR_BUF_SIZE        256
#define PAY_ATR_MAX             64
#define PAY_UID_MAX             32
#define PAY_PICC_COLLISION      6
#define PAY_KEYPAD_COUNT        11          /* 1-9, 0, 00 */

typedef enum
{
    PAY_STATE_AMOUNT = 0,
    PAY_STATE_WAIT_CARD,
} pay_state_t;

static lv_obj_t *s_amountLabel = NULL;
static lv_obj_t *s_statusLabel = NULL;
static lv_obj_t *s_detailLabel = NULL;
static lv_obj_t *s_keys[PAY_KEYPAD_COUNT];
static lv_obj_t *s_clearKey = NULL;
static lv_obj_t *s_delKey = NULL;
static lv_obj_t *s_payKey = NULL;
static lv_obj_t *s_cancelButton = NULL;

static lv_timer_t *s_timer = NULL;
static pay_state_t s_state = PAY_STATE_AMOUNT;
static unsigned long s_amountCents = 0;

static unsigned char s_trackBuf[3][PAY_MSR_BUF_SIZE];
static ST_MSC_TRACK s_track[3];
static unsigned char s_msrOpen = 0;
static unsigned char s_iccOpen = 0;
static unsigned char s_piccOpen = 0;

static void pay_refresh_amount(void);

static void pay_close_readers(void)
{
    if (s_msrOpen)
    {
        Lib_MscClose();
        s_msrOpen = 0;
    }

    if (s_iccOpen)
    {
        Lib_IccClose(ICC_SLOT_EMV_IC);
        s_iccOpen = 0;
    }

    if (s_piccOpen)
    {
        Lib_PiccClose();
        s_piccOpen = 0;
    }
}

static void pay_set_status(const char *text, lv_color_t color)
{
    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR(text));
        lv_obj_set_style_text_color(s_statusLabel, color, 0);
    }
}

static void pay_set_detail(const char *text)
{
    if (s_detailLabel != NULL)
    {
        lv_label_set_text(s_detailLabel, text);
    }
}

static void pay_show_obj(lv_obj_t *obj, bool show)
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

/* Switch keypad and action buttons by state so every operation can be done on the touch screen */
static void pay_update_controls(void)
{
    bool amount = (s_state == PAY_STATE_AMOUNT);
    bool waiting = (s_state == PAY_STATE_WAIT_CARD);
    int i;

    for (i = 0; i < PAY_KEYPAD_COUNT; i++)
    {
        pay_show_obj(s_keys[i], amount);
    }

    pay_show_obj(s_clearKey, amount);
    pay_show_obj(s_delKey, amount);
    pay_show_obj(s_payKey, amount);
    pay_show_obj(s_cancelButton, waiting);
}

static void pay_format_amount(char *out, unsigned int outSize)
{
    snprintf(out, outSize, "%lu.%02lu", s_amountCents / 100, s_amountCents % 100);
}

static void pay_refresh_amount(void)
{
    char text[24] = { 0 };

    pay_format_amount(text, sizeof(text));
    if (s_amountLabel != NULL)
    {
        lv_label_set_text(s_amountLabel, text);
    }
}

static void pay_add_digit(unsigned int digit)
{
    unsigned long next = s_amountCents * 10 + digit;

    if (next <= PAY_MAX_CENTS)
    {
        s_amountCents = next;
        pay_refresh_amount();
    }
}

static void pay_start_waiting(void);

static void pay_enter_pressed(void)
{
    if (s_state != PAY_STATE_AMOUNT)
    {
        return;
    }

    if (s_amountCents == 0)
    {
        pay_set_status("Enter amount first", APP_COLOR_WARN);
        return;
    }

    pay_start_waiting();
}

static void pay_cancel_waiting(void)
{
    pay_close_readers();
    s_state = PAY_STATE_AMOUNT;
    pay_set_status("Enter amount", APP_COLOR_SUB);
    pay_set_detail("");
    pay_update_controls();
}

static void pay_digit_cb(lv_event_t *e)
{
    int digit = (int)(intptr_t)lv_event_get_user_data(e);

    pay_add_digit((unsigned int)digit);
}

static void pay_double_zero_cb(lv_event_t *e)
{
    (void)e;

    pay_add_digit(0);
    pay_add_digit(0);
}

static void pay_clear_cb(lv_event_t *e)
{
    (void)e;

    s_amountCents = 0;
    pay_refresh_amount();
}

static void pay_del_cb(lv_event_t *e)
{
    (void)e;

    s_amountCents /= 10;
    pay_refresh_amount();
}

static void pay_pay_cb(lv_event_t *e)
{
    (void)e;

    pay_enter_pressed();
}

static void pay_cancel_cb(lv_event_t *e)
{
    (void)e;

    pay_cancel_waiting();
}

static void pay_format_hex(char *out, unsigned int outSize, const unsigned char *data, unsigned char len)
{
    unsigned int pos = 0;
    unsigned char i;

    if (out == NULL || outSize == 0)
    {
        return;
    }

    out[0] = '\0';
    for (i = 0; i < len && (pos + 4) < outSize; i++)
    {
        pos += (unsigned int)snprintf(out + pos, outSize - pos, "%02X ", data[i]);
    }
}

static void pay_card_found(const char *type, const char *cardData)
{
    app_page_t *page;

    Lib_Beep(BEEP_TIME_DEF);

    if (strcmp(type, "ICC") == 0)
    {
        /* Contact: keep ICC open (the EMV kernel sends/receives APDUs via Lib_IccCommand), close other readers */
        pay_set_status("Reading card...", APP_COLOR_ACCENT);
        lv_refr_now(NULL);

        if (s_msrOpen)
        {
            Lib_MscClose();
            s_msrOpen = 0;
        }

        if (s_piccOpen)
        {
            Lib_PiccClose();
            s_piccOpen = 0;
        }
    }
    else if (strcmp(type, "Contactless") == 0)
    {
        /* Contactless: keep the PICC field on (the kernel sends/receives APDUs via Lib_PiccCommand), close other readers */
        pay_set_status("Reading card...", APP_COLOR_ACCENT);
        lv_refr_now(NULL);

        if (s_msrOpen)
        {
            Lib_MscClose();
            s_msrOpen = 0;
        }

        if (s_iccOpen)
        {
            Lib_IccClose(ICC_SLOT_EMV_IC);
            s_iccOpen = 0;
        }
    }
    else
    {
        /* Magnetic stripe: pass track data to EmvNew's MccDemo for parsing, close all readers */
        EmvDemo_SetMsrTracks(&s_track[0], &s_track[1], &s_track[2]);
        pay_close_readers();
    }

    page = page_emv_open(s_amountCents, type, cardData);
    if (page != NULL)
    {
        app_page_push(page);
    }

    /* After returning to the payment page go straight back to amount entry so the next transaction can start */
    s_state = PAY_STATE_AMOUNT;
    pay_set_status("Enter amount", APP_COLOR_SUB);
    pay_set_detail("");
    pay_update_controls();
}

static void pay_detect_card(void)
{
    int rc;

    if (s_msrOpen)
    {
        rc = Lib_MscRead(&s_track[0], &s_track[1], &s_track[2]);
        if (rc == 1)
        {
            const char *track2 = (const char *)s_track[1].pucTrackBuf;

            if (track2[0] == '\0')
            {
                track2 = (const char *)s_track[0].pucTrackBuf;
            }

            pay_card_found("MSR", track2);
            return;
        }
    }

    if (s_iccOpen && Lib_IccIsPresent(ICC_SLOT_EMV_IC) == 1)
    {
        unsigned char atr[PAY_ATR_MAX];
        unsigned char atrLen = PAY_ATR_MAX;
        unsigned char uid[PAY_UID_MAX];
        unsigned char uidLen = PAY_UID_MAX;
        char hexText[120] = { 0 };

        rc = Lib_IccInquiry(ICC_SLOT_EMV_IC, atr, &atrLen, uid, &uidLen);
        if (rc > 0)
        {
            if (uidLen > 0)
            {
                pay_format_hex(hexText, sizeof(hexText), uid, uidLen);
            }
            else
            {
                pay_format_hex(hexText, sizeof(hexText), atr, atrLen);
            }
            pay_card_found("ICC", hexText);
            return;
        }
    }

    if (s_piccOpen)
    {
        unsigned char atr[PAY_ATR_MAX];
        unsigned char atrLen = PAY_ATR_MAX;
        unsigned char uid[PAY_UID_MAX];
        unsigned char uidLen = PAY_UID_MAX;
        char hexText[120] = { 0 };

        rc = Lib_PiccInquiry(atr, &atrLen, uid, &uidLen);
        if (rc == PAY_PICC_COLLISION)
        {
            pay_set_status("Multiple cards, remove one", APP_COLOR_WARN);
            return;
        }

        if (rc > 0)
        {
            if (uidLen > 0)
            {
                pay_format_hex(hexText, sizeof(hexText), uid, uidLen);
            }
            else
            {
                pay_format_hex(hexText, sizeof(hexText), atr, atrLen);
            }
            pay_card_found("Contactless", hexText);
            return;
        }
    }
}

static void pay_start_waiting(void)
{
    pay_close_readers();
    memset(s_trackBuf, 0, sizeof(s_trackBuf));
    s_track[0].pucTrackBuf = s_trackBuf[0];
    s_track[0].ucTrackLen = 0;
    s_track[0].ucTrackValid = 0;
    s_track[1].pucTrackBuf = s_trackBuf[1];
    s_track[1].ucTrackLen = 0;
    s_track[1].ucTrackValid = 0;
    s_track[2].pucTrackBuf = s_trackBuf[2];
    s_track[2].ucTrackLen = 0;
    s_track[2].ucTrackValid = 0;

    s_msrOpen = (Lib_MscOpen() == 0) ? 1 : 0;
    s_iccOpen = (Lib_IccOpen(ICC_SLOT_EMV_IC) == 0) ? 1 : 0;
    s_piccOpen = (Lib_PiccOpen() == 0) ? 1 : 0;

    if (!s_msrOpen && !s_iccOpen && !s_piccOpen)
    {
        pay_set_status("Card reader init failed", APP_COLOR_WARN);
        return;
    }

    s_state = PAY_STATE_WAIT_CARD;
    pay_set_status("Swipe / Insert / Tap card", APP_COLOR_ACCENT);
    pay_set_detail("");
    pay_update_controls();
}

static void pay_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_state == PAY_STATE_WAIT_CARD)
    {
        pay_detect_card();
    }
}

static lv_obj_t *pay_make_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
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
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_center(label);

    lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);

    return btn;
}

/* Digit keys: white card style, to avoid confusion with the action buttons below */
static lv_obj_t *pay_make_key(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                              lv_coord_t w, lv_coord_t h, const char *text)
{
    lv_obj_t *key = app_card(parent, x, y, w, h);
    lv_obj_t *label = NULL;

    if (key == NULL)
    {
        return NULL;
    }

    label = lv_label_create(key);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(24), 0);
    lv_obj_center(label);

    lv_obj_add_flag(key, LV_OBJ_FLAG_HIDDEN);

    return key;
}

static void pay_create(app_page_t *page)
{
    static const char *keyText[PAY_KEYPAD_COUNT] =
    {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "00"
    };
    lv_obj_t *amountCard = app_card(page->body, APP_ROW_X, APP_ROW_START_Y, APP_ROW_W, 132);
    lv_obj_t *detailCard = NULL;
    lv_obj_t *hint = NULL;
    lv_coord_t y = 334;
    int i;

    if (amountCard != NULL)
    {
        lv_obj_clear_flag(amountCard, LV_OBJ_FLAG_CLICKABLE);

        s_amountLabel = lv_label_create(amountCard);
        lv_label_set_text(s_amountLabel, "0.00");
        lv_obj_set_style_text_color(s_amountLabel, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_amountLabel, app_font(40), 0);
        lv_obj_align(s_amountLabel, LV_ALIGN_TOP_MID, 0, 12);

        s_statusLabel = lv_label_create(amountCard);
        lv_label_set_text(s_statusLabel, LIB_TR("Enter amount"));
        lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_SUB, 0);
        lv_obj_set_style_text_font(s_statusLabel, app_font(18), 0);
        lv_obj_align(s_statusLabel, LV_ALIGN_BOTTOM_MID, 0, -14);
    }

    detailCard = app_card(page->body, APP_ROW_X, 160, APP_ROW_W, 160);
    if (detailCard != NULL)
    {
        lv_obj_clear_flag(detailCard, LV_OBJ_FLAG_CLICKABLE);

        s_detailLabel = lv_label_create(detailCard);
        lv_label_set_text(s_detailLabel, "");
        lv_label_set_long_mode(s_detailLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(s_detailLabel, APP_ROW_W - 48);
        lv_obj_set_style_text_color(s_detailLabel, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_detailLabel, app_font(16), 0);
        lv_obj_align(s_detailLabel, LV_ALIGN_TOP_LEFT, 24, 20);
    }

    /* Keypad 4 columns: 3 digit columns + 1 function column; rows 1-3 hold 1-9, row 4 holds 0/00 */
    for (i = 0; i < 9; i++)
    {
        lv_coord_t kx = APP_ROW_X + (lv_coord_t)(i % 3) * 115;
        lv_coord_t ky = y + (lv_coord_t)(i / 3) * 74;

        s_keys[i] = pay_make_key(page->body, kx, ky, 103, 64, keyText[i]);
        if (s_keys[i] != NULL)
        {
            lv_obj_add_event_cb(s_keys[i], pay_digit_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(i + 1));
        }
    }

    s_keys[9] = pay_make_key(page->body, APP_ROW_X, y + 222, 218, 64, keyText[9]);
    if (s_keys[9] != NULL)
    {
        lv_obj_add_event_cb(s_keys[9], pay_digit_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    }

    s_keys[10] = pay_make_key(page->body, APP_ROW_X + 230, y + 222, 103, 64, keyText[10]);
    if (s_keys[10] != NULL)
    {
        lv_obj_add_event_cb(s_keys[10], pay_double_zero_cb, LV_EVENT_CLICKED, NULL);
    }

    s_clearKey = pay_make_key(page->body, APP_ROW_X + 345, y, 103, 64, LIB_TR("CLR"));
    if (s_clearKey != NULL)
    {
        lv_obj_add_event_cb(s_clearKey, pay_clear_cb, LV_EVENT_CLICKED, NULL);
    }

    s_delKey = pay_make_key(page->body, APP_ROW_X + 345, y + 74, 103, 64, LIB_TR("DEL"));
    if (s_delKey != NULL)
    {
        lv_obj_add_event_cb(s_delKey, pay_del_cb, LV_EVENT_CLICKED, NULL);
    }

    s_payKey = pay_make_button(page->body, APP_ROW_X + 345, y + 148, 103, 138,
                               LIB_TR("PAY"), APP_COLOR_ACCENT);
    if (s_payKey != NULL)
    {
        lv_obj_add_event_cb(s_payKey, pay_pay_cb, LV_EVENT_CLICKED, NULL);
    }

    s_cancelButton = pay_make_button(page->body, APP_ROW_X, y + 148, 448, 138,
                                     LIB_TR("Cancel"), APP_COLOR_ACCENT);
    if (s_cancelButton != NULL)
    {
        lv_obj_add_event_cb(s_cancelButton, pay_cancel_cb, LV_EVENT_CLICKED, NULL);
    }

    hint = lv_label_create(page->body);
    lv_label_set_text(hint, LIB_TR("Touch keypad to enter amount, PAY to start. BACK to exit"));
    lv_obj_set_style_text_color(hint, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(hint, app_font(14), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_state = PAY_STATE_AMOUNT;
    s_amountCents = 0;
    s_timer = lv_timer_create(pay_timer_cb, PAY_POLL_MS, NULL);

    pay_update_controls();
}

static void pay_destroy(app_page_t *page)
{
    int i;

    (void)page;

    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    pay_close_readers();

    s_amountLabel = NULL;
    s_statusLabel = NULL;
    s_detailLabel = NULL;

    for (i = 0; i < PAY_KEYPAD_COUNT; i++)
    {
        s_keys[i] = NULL;
    }

    s_clearKey = NULL;
    s_delKey = NULL;
    s_payKey = NULL;
    s_cancelButton = NULL;
}

static bool pay_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (s_state == PAY_STATE_AMOUNT)
    {
        if (ev->param >= KB_DIGIT_ZERO && ev->param <= KB_DIGIT_NINE)
        {
            pay_add_digit((unsigned int)(ev->param - KB_DIGIT_ZERO));
            return true;
        }

        if (ev->param == KB_CLEAR)
        {
            s_amountCents = 0;
            pay_refresh_amount();
            return true;
        }

        if (ev->param == KB_ENTER)
        {
            pay_enter_pressed();
            return true;
        }

        return false;
    }

    if (s_state == PAY_STATE_WAIT_CARD && ev->param == KB_CLEAR)
    {
        pay_cancel_waiting();
        return true;
    }

    return false;
}

static app_page_t s_paymentPage =
{
    .id         = "payment",
    .title      = "PAYMENT",
    .on_create  = pay_create,
    .on_event   = pay_event,
    .on_destroy = pay_destroy,
    .user_data  = NULL,
};

app_page_t *page_payment_get(void)
{
    return &s_paymentPage;
}
