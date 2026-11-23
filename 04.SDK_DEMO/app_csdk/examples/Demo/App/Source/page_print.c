/**********************************************************************************************************
 * File Name     : page_print.c
 * Description   : Print test page
 *
 *   Printing must run synchronously in the LVGL/UI thread like the original PrintTest():
 *   the thermal printer driver holds global state internally, calling from a worker thread causes a system reboot.
 *   "Printing..." is shown on screen before printing, avoiding no UI feedback while feeding paper.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibPrint.h"
#include "LibCharConvert.h"   /* Lib_Bcd2Asc() */

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_statusLabel = NULL;
static lv_obj_t *s_resultLabel = NULL;
static lv_obj_t *s_gradeValue = NULL;
static lv_obj_t *s_printButton = NULL;
static unsigned char s_grade = 1;

static int print_line(unsigned char align, const char *text)
{
    return Lib_PrnStrEx(align, (char *)text, (unsigned long)strlen(text));
}

static int print_test_page(void)
{
    unsigned char aucTimeBcd[7] = { 0 };
    char acTimeStr[13] = { 0 };
    char acLine[40] = { 0 };
    int rc = RET_OK;

    rc = Lib_PrnOpen();
    if (rc != RET_OK)
    {
        return rc;
    }

    Lib_PrnSetSpace(8);

    /* Same as original PrintTest(): title FONT32, body FONT24, neither scaled up.
       Scaling font up makes line width exceed 384 dots; print buffer overflow makes driver misjudge status */
    Lib_PrnSetFont(FONT32, 1, 1);
    print_line(ALIGN_M, "PRINTER TEST");

    Lib_PrnSetFont(FONT24, 1, 1);
    print_line(ALIGN_M, "----------------------");

    print_line(ALIGN_L, "DEVICE : D30R");
    print_line(ALIGN_L, "RESULT : OK");

    Lib_GetDateTime(aucTimeBcd);
    Lib_Bcd2Asc(aucTimeBcd, acTimeStr, 6);
    snprintf(acLine, sizeof(acLine), "DATE   : 20%c%c-%c%c-%c%c %c%c:%c%c",
             acTimeStr[0], acTimeStr[1], acTimeStr[2], acTimeStr[3],
             acTimeStr[4], acTimeStr[5], acTimeStr[6], acTimeStr[7],
             acTimeStr[8], acTimeStr[9]);
    print_line(ALIGN_L, acLine);

    print_line(ALIGN_M, "----------------------");

    /* Content is short, paper head has not reached the paper-out detection point, feed two lines before closing,
       otherwise Lib_PrnClose() misjudges "paper not in position" as paper out */
    Lib_PrnPaperForward(3000);
    Lib_PrnPaperForward(3000);

    rc = Lib_PrnClose();

    /* Re-check the real status: 0 means normal, less than 0 means paper out */
    if (rc == RET_ERR_TPH_NO_PAPER && Lib_PrnCheckStatus() == 0)
    {
        rc = RET_OK;
    }

    return rc;
}

static void print_show_busy(void)
{
    lv_obj_t *label = NULL;

    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR("Printing..."));
    }

    if (s_resultLabel != NULL)
    {
        lv_label_set_text(s_resultLabel, "-");
        lv_obj_set_style_text_color(s_resultLabel, APP_COLOR_SUB, 0);
    }

    if (s_printButton != NULL)
    {
        label = lv_obj_get_child(s_printButton, 0);
        if (label != NULL)
        {
            lv_label_set_text(label, LIB_TR("Printing..."));
        }
    }

    /* UI thread is occupied during printing, force a refresh first */
    lv_refr_now(NULL);
}

static void print_show_result(int rc)
{
    char buf[24] = { 0 };
    lv_obj_t *label = NULL;

    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR("Completed"));
    }

    if (s_resultLabel != NULL)
    {
        if (rc == RET_OK)
        {
            lv_label_set_text(s_resultLabel, LIB_TR("OK"));
            lv_obj_set_style_text_color(s_resultLabel, APP_COLOR_SUCCESS, 0);
        }
        else if (rc == RET_ERR_TPH_NO_PAPER)
        {
            lv_label_set_text(s_resultLabel, LIB_TR("Out of paper"));
            lv_obj_set_style_text_color(s_resultLabel, APP_COLOR_WARN, 0);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Error %d", rc);
            lv_label_set_text(s_resultLabel, buf);
            lv_obj_set_style_text_color(s_resultLabel, APP_COLOR_WARN, 0);
        }
    }

    if (s_printButton != NULL)
    {
        label = lv_obj_get_child(s_printButton, 0);
        if (label != NULL)
        {
            lv_label_set_text(label, LIB_TR("Print Test Page"));
        }
    }
}

static void print_start(void)
{
    print_show_busy();
    print_show_result(print_test_page());
}

static void print_start_cb(lv_event_t *e)
{
    (void)e;

    print_start();
}

static void print_grade_show(void)
{
    char text[8] = { 0 };

    if (s_gradeValue != NULL)
    {
        snprintf(text, sizeof(text), "%u/%u", (unsigned int)s_grade, PT_PRINT_GRADE_MAX);
        lv_label_set_text(s_gradeValue, text);
    }
}

static void print_grade_step(int delta)
{
    int next = (int)s_grade + delta;

    if (next > PT_PRINT_GRADE_MAX)
    {
        next = 1;
    }
    else if (next < 1)
    {
        next = PT_PRINT_GRADE_MAX;
    }

    s_grade = (unsigned char)next;
    Lib_PrnSetGrade(s_grade);
    print_grade_show();
}

static void print_grade_inc_cb(lv_event_t *e)
{
    (void)e;

    print_grade_step(1);
}

static void print_grade_dec_cb(lv_event_t *e)
{
    (void)e;

    print_grade_step(-1);
}

static lv_obj_t *print_small_button(lv_obj_t *parent, const char *text, lv_coord_t xOfs)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 52, 52);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, xOfs, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_center(label);

    return btn;
}

static void print_create_grade(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *card = app_card(parent, APP_ROW_X, y, APP_ROW_W, APP_ROW_H);
    lv_obj_t *name = NULL;
    lv_obj_t *btn = NULL;

    if (card == NULL)
    {
        return;
    }

    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    name = lv_label_create(card);
    lv_label_set_text(name, LIB_TR("Print Grade"));
    lv_obj_set_style_text_color(name, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(name, app_font(20), 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 24, 0);

    s_gradeValue = lv_label_create(card);
    lv_obj_set_width(s_gradeValue, 88);
    lv_obj_set_style_text_align(s_gradeValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_gradeValue, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_gradeValue, app_font(22), 0);
    lv_obj_align(s_gradeValue, LV_ALIGN_RIGHT_MID, -152, 0);

    btn = print_small_button(card, "+", -16);
    if (btn != NULL)
    {
        lv_obj_add_event_cb(btn, print_grade_inc_cb, LV_EVENT_CLICKED, NULL);
    }

    btn = print_small_button(card, "-", -80);
    if (btn != NULL)
    {
        lv_obj_add_event_cb(btn, print_grade_dec_cb, LV_EVENT_CLICKED, NULL);
    }
}

static lv_obj_t *print_create_button(lv_obj_t *parent, lv_coord_t y, const char *text)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, APP_ROW_X, y);
    lv_obj_set_size(btn, APP_ROW_W, 60);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, APP_COLOR_ACCENT_2, LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, app_font(20), 0);
    lv_obj_center(label);

    return btn;
}

static void print_create(app_page_t *page)
{
    lv_obj_t *tip = NULL;

    s_statusLabel = app_row(page->body, APP_ROW_START_Y, "Printer");
    print_create_grade(page->body, APP_ROW_START_Y + APP_ROW_H + APP_ROW_GAP);
    s_resultLabel = app_row(page->body, APP_ROW_START_Y + 2 * (APP_ROW_H + APP_ROW_GAP), "Result");

    s_grade = Lib_PrnGetGrade();
    if (s_grade < 1 || s_grade > PT_PRINT_GRADE_MAX)
    {
        s_grade = 1;
    }
    print_grade_show();

    s_printButton = print_create_button(page->body, 268, LIB_TR("Print Test Page"));
    if (s_printButton != NULL)
    {
        lv_obj_add_event_cb(s_printButton, print_start_cb, LV_EVENT_CLICKED, NULL);
    }

    tip = lv_label_create(page->body);
    lv_label_set_text(tip, LIB_TR("Make sure paper is loaded before printing."));
    lv_obj_set_style_text_color(tip, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(tip, app_font(14), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -12);

    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR("Idle"));
    }
}

static void print_destroy(app_page_t *page)
{
    (void)page;

    s_statusLabel = NULL;
    s_resultLabel = NULL;
    s_gradeValue = NULL;
    s_printButton = NULL;
}

static bool print_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param == KB_ENTER)
    {
        print_start();
        return true;
    }

    return false;
}

static app_page_t s_printPage =
{
    .id         = "print",
    .title      = "PRINT TEST",
    .on_create  = print_create,
    .on_event   = print_event,
    .on_destroy = print_destroy,
    .user_data  = NULL,
};

app_page_t *page_print_get(void)
{
    return &s_printPage;
}
