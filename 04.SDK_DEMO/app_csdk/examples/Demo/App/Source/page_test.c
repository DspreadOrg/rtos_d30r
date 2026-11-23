/**********************************************************************************************************
 * File Name     : page_test.c
 * Description   : Hardware test page (magnetic stripe / IC / contactless card / barcode scan / e-signature)
 *
 *   Card reading polls with lv_timer; all calls are non-blocking and do not occupy the LVGL thread;
 *   card readers are closed on page exit or test item switch.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibApi.h"
#include "LibCards.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MSR_BUF_SIZE       256
#define TEST_ATR_MAX            64
#define TEST_UID_MAX            32
#define TEST_POLL_MS            150
#define TEST_PICC_COLLISION     6

typedef enum
{
    TEST_MODE_NONE = 0,
    TEST_MODE_MSR,
    TEST_MODE_ICC,
    TEST_MODE_PICC,
} test_mode_t;

static lv_obj_t *s_statusLabel = NULL;
static lv_obj_t *s_dataLabel = NULL;
static lv_obj_t *s_stopButton = NULL;
static lv_timer_t *s_timer = NULL;
static test_mode_t s_mode = TEST_MODE_NONE;

static unsigned char s_trackBuf[3][TEST_MSR_BUF_SIZE];
static ST_MSC_TRACK s_track[3];

static void test_format_hex(char *out, unsigned int outSize, const unsigned char *data, unsigned char len)
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

static void test_set_status(const char *text, lv_color_t color)
{
    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR(text));
        lv_obj_set_style_text_color(s_statusLabel, color, 0);
    }
}

static void test_set_data(const char *text)
{
    if (s_dataLabel != NULL)
    {
        lv_label_set_text(s_dataLabel, text);
    }
}

static void test_stop_device(void)
{
    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    switch (s_mode)
    {
    case TEST_MODE_MSR:
        Lib_MscClose();
        break;

    case TEST_MODE_ICC:
        Lib_IccClose(ICC_SLOT_EMV_IC);
        break;

    case TEST_MODE_PICC:
        Lib_PiccClose();
        break;

    default:
        break;
    }

    s_mode = TEST_MODE_NONE;
}

static void test_finish(const char *status, lv_color_t color, const char *data)
{
    test_stop_device();
    test_set_status(status, color);
    test_set_data(data);

    if (s_stopButton != NULL)
    {
        lv_obj_add_flag(s_stopButton, LV_OBJ_FLAG_HIDDEN);
    }
}

static void test_poll_cb(lv_timer_t *timer)
{
    static char hexText[200];
    int rc;

    (void)timer;

    if (s_mode == TEST_MODE_NONE)
    {
        return;
    }

    if (s_mode == TEST_MODE_MSR)
    {
        rc = Lib_MscRead(&s_track[0], &s_track[1], &s_track[2]);
        if (rc == 1)
        {
            const char *track = (const char *)s_track[1].pucTrackBuf;

            if (track[0] == '\0')
            {
                track = (const char *)s_track[0].pucTrackBuf;
            }

            Lib_Beep(BEEP_TIME_DEF);
            test_finish("MSR OK", APP_COLOR_SUCCESS, track);
            return;
        }

        if (rc < 0)
        {
            test_finish("MSR read failed", APP_COLOR_WARN, "");
        }
        return;
    }

    if (s_mode == TEST_MODE_ICC)
    {
        unsigned char atr[TEST_ATR_MAX];
        unsigned char atrLen = TEST_ATR_MAX;
        unsigned char uid[TEST_UID_MAX];
        unsigned char uidLen = TEST_UID_MAX;

        if (Lib_IccIsPresent(ICC_SLOT_EMV_IC) != 1)
        {
            return;
        }

        rc = Lib_IccInquiry(ICC_SLOT_EMV_IC, atr, &atrLen, uid, &uidLen);
        if (rc > 0)
        {
            Lib_Beep(BEEP_TIME_DEF);
            test_format_hex(hexText, sizeof(hexText), atr, atrLen);
            test_finish("ICC OK", APP_COLOR_SUCCESS, hexText);
        }
        return;
    }

    /* Contactless card */
    {
        unsigned char atr[TEST_ATR_MAX];
        unsigned char atrLen = TEST_ATR_MAX;
        unsigned char uid[TEST_UID_MAX];
        unsigned char uidLen = TEST_UID_MAX;

        rc = Lib_PiccInquiry(atr, &atrLen, uid, &uidLen);
        if (rc == TEST_PICC_COLLISION)
        {
            test_finish("Multiple cards", APP_COLOR_WARN, "");
            return;
        }

        if (rc > 0)
        {
            Lib_Beep(BEEP_TIME_DEF);
            test_format_hex(hexText, sizeof(hexText), uid, uidLen);
            test_finish("PICC OK", APP_COLOR_SUCCESS, hexText);
        }
    }
}

static void test_start(test_mode_t mode)
{
    test_stop_device();

    test_set_data("");
    test_set_status("Reading...", APP_COLOR_ACCENT);

    switch (mode)
    {
    case TEST_MODE_MSR:
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

        if (Lib_MscOpen() != 0)
        {
            test_finish("MSR open failed", APP_COLOR_WARN, "");
            return;
        }
        test_set_status("Swipe card...", APP_COLOR_ACCENT);
        break;

    case TEST_MODE_ICC:
        if (Lib_IccOpen(ICC_SLOT_EMV_IC) != 0)
        {
            test_finish("ICC open failed", APP_COLOR_WARN, "");
            return;
        }
        test_set_status("Insert IC card...", APP_COLOR_ACCENT);
        break;

    case TEST_MODE_PICC:
        if (Lib_PiccOpen() != 0)
        {
            test_finish("PICC open failed", APP_COLOR_WARN, "");
            return;
        }
        test_set_status("Tap card...", APP_COLOR_ACCENT);
        break;

    default:
        return;
    }

    s_mode = mode;
    s_timer = lv_timer_create(test_poll_cb, TEST_POLL_MS, NULL);

    if (s_stopButton != NULL)
    {
        lv_obj_clear_flag(s_stopButton, LV_OBJ_FLAG_HIDDEN);
    }
}

static void test_stop_cb(lv_event_t *e)
{
    (void)e;

    test_stop_device();
    test_set_data("");
    test_set_status("Select a card test", APP_COLOR_SUB);
    if (s_stopButton != NULL)
    {
        lv_obj_add_flag(s_stopButton, LV_OBJ_FLAG_HIDDEN);
    }
}

static void test_mode_clicked_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index == 3)
    {
        app_page_push(page_scan_get());
        return;
    }

    if (index == 4)
    {
        app_page_push(page_signature_get());
        return;
    }

    test_start((test_mode_t)(index + 1));
}

static lv_obj_t *test_build_button(lv_obj_t *parent, lv_coord_t y, const char *title, int index)
{
    lv_obj_t *card = app_card(parent, APP_ROW_X, y, APP_ROW_W, 56);
    lv_obj_t *label = NULL;

    if (card == NULL)
    {
        return NULL;
    }

    label = lv_label_create(card);
    lv_label_set_text(label, LIB_TR(title));
    lv_obj_set_style_text_color(label, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 24, 0);

    lv_obj_add_event_cb(card, test_mode_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    return card;
}

static void test_create(app_page_t *page)
{
    lv_obj_t *resultCard = NULL;
    lv_obj_t *stopLabel = NULL;

    test_build_button(page->body, APP_ROW_START_Y, "1  Magnetic Stripe", 0);
    test_build_button(page->body, APP_ROW_START_Y + 64, "2  IC Card", 1);
    test_build_button(page->body, APP_ROW_START_Y + 128, "3  Contactless", 2);
    test_build_button(page->body, APP_ROW_START_Y + 192, "4  QR Scan", 3);
    test_build_button(page->body, APP_ROW_START_Y + 256, "5  E-Signature", 4);

    resultCard = app_card(page->body, APP_ROW_X, APP_ROW_START_Y + 324, APP_ROW_W, 244);
    if (resultCard != NULL)
    {
        lv_obj_clear_flag(resultCard, LV_OBJ_FLAG_CLICKABLE);

        s_statusLabel = lv_label_create(resultCard);
        lv_label_set_text(s_statusLabel, LIB_TR("Select a card test"));
        lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_SUB, 0);
        lv_obj_set_style_text_font(s_statusLabel, app_font(20), 0);
        lv_obj_align(s_statusLabel, LV_ALIGN_TOP_LEFT, 24, 20);

        s_dataLabel = lv_label_create(resultCard);
        lv_label_set_text(s_dataLabel, "");
        lv_label_set_long_mode(s_dataLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(s_dataLabel, APP_ROW_W - 48);
        lv_obj_set_style_text_color(s_dataLabel, APP_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_dataLabel, app_font(16), 0);
        lv_obj_align(s_dataLabel, LV_ALIGN_TOP_LEFT, 24, 64);
    }

    /* Touch stop button: shown while a test runs, hidden after finish or stop */
    s_stopButton = lv_obj_create(page->body);
    if (s_stopButton != NULL)
    {
        lv_obj_remove_style_all(s_stopButton);
        lv_obj_set_pos(s_stopButton, APP_ROW_X, APP_ROW_START_Y + 600);
        lv_obj_set_size(s_stopButton, APP_ROW_W, 56);
        lv_obj_set_style_bg_color(s_stopButton, APP_COLOR_WARN, 0);
        lv_obj_set_style_bg_opa(s_stopButton, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_stopButton, 10, 0);
        lv_obj_clear_flag(s_stopButton, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_stopButton, LV_OBJ_FLAG_HIDDEN);

        stopLabel = lv_label_create(s_stopButton);
        lv_label_set_text(stopLabel, LIB_TR("Stop"));
        lv_obj_set_style_text_color(stopLabel, lv_color_white(), 0);
        lv_obj_set_style_text_font(stopLabel, app_font(20), 0);
        lv_obj_center(stopLabel);

        lv_obj_add_event_cb(s_stopButton, test_stop_cb, LV_EVENT_CLICKED, NULL);
    }

    s_mode = TEST_MODE_NONE;
}

static void test_destroy(app_page_t *page)
{
    (void)page;

    test_stop_device();
    s_statusLabel = NULL;
    s_dataLabel = NULL;
    s_stopButton = NULL;
}

static bool test_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param >= KB_DIGIT_ONE && ev->param <= KB_DIGIT_THREE)
    {
        test_start((test_mode_t)(ev->param - KB_DIGIT_ONE + 1));
        return true;
    }

    if (ev->param == KB_DIGIT_FOUR)
    {
        app_page_push(page_scan_get());
        return true;
    }

    if (ev->param == KB_DIGIT_FIVE)
    {
        app_page_push(page_signature_get());
        return true;
    }

    return false;
}

static app_page_t s_testPage =
{
    .id         = "test",
    .title      = "TEST",
    .on_create  = test_create,
    .on_event   = test_event,
    .on_destroy = test_destroy,
    .user_data  = NULL,
};

app_page_t *page_test_get(void)
{
    return &s_testPage;
}
