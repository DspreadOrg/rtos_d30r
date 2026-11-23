/**********************************************************************************************************
 * File Name     : page_signature.c
 * Description   : Electronic signature test page
 *
 *   Write a signature by hand in the white box, Clear erases it, Print prints it.
 *   Pressing Print without a signature only shows "Please sign first" and does not feed paper.
 *   Printing uses Lib_PrnOneRowOfRawData() to send a 384-dot monochrome bitmap row by row;
 *   like the print test page, the print driver has internal global state and must run synchronously in the LVGL/UI thread.
 **********************************************************************************************************/

#include "app_pages.h"
#include "app_ui.h"
#include "LibApi.h"
#include "LibPrint.h"

#include <stdio.h>
#include <string.h>

#define SIG_CARD_X          APP_ROW_X
#define SIG_CARD_Y          16
#define SIG_CARD_W          APP_ROW_W
#define SIG_CARD_H          430
#define SIG_MAX_POINTS      1024        /* about 5KB static memory, enough to fill the whole signature area */
#define SIG_INK_WIDTH       3
#define SIG_PRINT_DOT       LTP_MAX_DOT /* printer width of 384 dots */
#define SIG_PRINT_GRADE     3

static lv_point_t s_points[SIG_MAX_POINTS];
static unsigned char s_strokeBreak[SIG_MAX_POINTS];  /* 1 means this point starts a new stroke */
static int s_pointCount = 0;

static lv_obj_t *s_canvas = NULL;
static lv_obj_t *s_hintLabel = NULL;
static lv_obj_t *s_statusLabel = NULL;

static void sig_set_status(const char *text, lv_color_t color)
{
    if (s_statusLabel != NULL)
    {
        lv_label_set_text(s_statusLabel, LIB_TR(text));
        lv_obj_set_style_text_color(s_statusLabel, color, 0);
    }
}

static void sig_update_hint(void)
{
    if (s_hintLabel == NULL)
    {
        return;
    }

    if (s_pointCount == 0)
    {
        lv_obj_clear_flag(s_hintLabel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_hintLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void sig_add_point(lv_coord_t x, lv_coord_t y, unsigned char newStroke)
{
    if (s_pointCount >= SIG_MAX_POINTS)
    {
        return;
    }

    if (x < 0)
    {
        x = 0;
    }
    else if (x >= SIG_CARD_W)
    {
        x = SIG_CARD_W - 1;
    }

    if (y < 0)
    {
        y = 0;
    }
    else if (y >= SIG_CARD_H)
    {
        y = SIG_CARD_H - 1;
    }

    s_points[s_pointCount].x = x;
    s_points[s_pointCount].y = y;
    s_strokeBreak[s_pointCount] = newStroke;
    s_pointCount++;

    if (s_pointCount == 1)
    {
        sig_update_hint();
    }

    if (s_canvas != NULL)
    {
        lv_obj_invalidate(s_canvas);
    }
}

static void sig_touch_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    lv_area_t area;
    unsigned char newStroke = (lv_event_get_code(e) == LV_EVENT_PRESSED) ? 1 : 0;

    if (indev == NULL)
    {
        return;
    }

    lv_indev_get_point(indev, &point);
    lv_obj_get_coords(obj, &area);

    sig_add_point(point.x - area.x1, point.y - area.y1, newStroke);
}

/* Redraw: draw the recorded trajectory as polylines.
   Recorded points are relative to the signature box, while LVGL's draw_ctx uses absolute screen coordinates,
   so the box's own absolute origin must be added before drawing, otherwise the strokes are shifted as a whole. */
static void sig_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *drawCtx = lv_event_get_draw_ctx(e);
    lv_draw_line_dsc_t dsc;
    lv_area_t area;
    int i;

    if (s_pointCount < 2)
    {
        return;
    }

    lv_obj_get_coords(obj, &area);

    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x12233A);
    dsc.width = SIG_INK_WIDTH;
    dsc.opa = LV_OPA_COVER;
    dsc.round_start = 1;
    dsc.round_end = 1;

    for (i = 1; i < s_pointCount; i++)
    {
        lv_point_t p1;
        lv_point_t p2;

        if (s_strokeBreak[i])
        {
            continue;
        }

        p1.x = s_points[i - 1].x + area.x1;
        p1.y = s_points[i - 1].y + area.y1;
        p2.x = s_points[i].x + area.x1;
        p2.y = s_points[i].y + area.y1;

        lv_draw_line(drawCtx, &dsc, &p1, &p2);
    }
}

static void sig_clear(void)
{
    s_pointCount = 0;
    sig_update_hint();
    sig_set_status("Sign in the box, then press Print", APP_COLOR_SUB);

    if (s_canvas != NULL)
    {
        lv_obj_invalidate(s_canvas);
    }
}

/* ---------------- Print: proportionally map screen coordinates to a 384-dot monochrome bitmap ---------------- */

static int sig_map_x(lv_coord_t x)
{
    return (int)(((long)x * SIG_PRINT_DOT) / SIG_CARD_W);
}

static int sig_map_y(lv_coord_t y)
{
    return (int)(((long)y * SIG_PRINT_DOT) / SIG_CARD_W);  /* proportional scaling */
}

static int sig_print_rows(void)
{
    int rows = sig_map_y(SIG_CARD_H);

    return (rows < 1) ? 1 : rows;
}

static void sig_set_pixel(unsigned char *rowBuf, int x)
{
    if ((x < 0) || (x >= SIG_PRINT_DOT))
    {
        return;
    }

    rowBuf[x >> 3] |= (unsigned char)(0x80 >> (x & 0x07));
}

static void sig_render_row(int row, unsigned char *rowBuf)
{
    int i;

    memset(rowBuf, 0, LTP_MAX_BYTE);

    for (i = 1; i < s_pointCount; i++)
    {
        int x1, y1, x2, y2;
        int ymin, ymax;
        int xa, xb, x;

        if (s_strokeBreak[i])
        {
            continue;
        }

        x1 = sig_map_x(s_points[i - 1].x);
        y1 = sig_map_y(s_points[i - 1].y);
        x2 = sig_map_x(s_points[i].x);
        y2 = sig_map_y(s_points[i].y);

        ymin = (y1 < y2) ? y1 : y2;
        ymax = (y1 < y2) ? y2 : y1;

        if ((row < ymin) || (row > ymax))
        {
            continue;   /* this row is not within the vertical range of this segment */
        }

        if (y1 == y2)
        {
            /* Horizontal segment: fill the whole span */
            xa = (x1 < x2) ? x1 : x2;
            xb = (x1 < x2) ? x2 : x1;
        }
        else
        {
            /* Intersect the segment with the current row band, taking x at both ends,
               so nearly horizontal slanted lines are also filled completely and do not degenerate into dots */
            int ya = (row > ymin) ? row : ymin;
            int yb = ((row + 1) < ymax) ? (row + 1) : ymax;
            long dy = (long)(y2 - y1);

            xa = x1 + (int)(((long)(x2 - x1) * (ya - y1)) / dy);
            xb = x1 + (int)(((long)(x2 - x1) * (yb - y1)) / dy);
        }

        if (xa > xb)
        {
            int tmp = xa;
            xa = xb;
            xb = tmp;
        }

        /* 3-dot pen width, close to the thickness drawn on screen */
        xa -= 1;
        xb += 1;

        for (x = xa; x <= xb; x++)
        {
            sig_set_pixel(rowBuf, x);
        }
    }
}

static int sig_print_bitmap(void)
{
    unsigned char rowBuf[LTP_MAX_BYTE];
    int rows = sig_print_rows();
    int rc;
    int i;

    rc = Lib_PrnCheckStatus();
    if (rc != RET_OK)
    {
        return rc;
    }

    rc = Lib_PrnOpen();
    if (rc != RET_OK)
    {
        return rc;
    }

    Lib_PrnSetGrade(SIG_PRINT_GRADE);

    for (i = 0; i < rows; i++)
    {
        sig_render_row(i, rowBuf);
        Lib_PrnOneRowOfRawData(rowBuf);
    }

    /* Same as the print test page: feed paper before closing, so a paper head not yet in place is not misjudged as out of paper */
    Lib_PrnPaperForward(3000);
    Lib_PrnPaperForward(3000);

    rc = Lib_PrnClose();

    if ((rc == RET_ERR_TPH_NO_PAPER) && (Lib_PrnCheckStatus() == 0))
    {
        rc = RET_OK;
    }

    return rc;
}

static void sig_print(void)
{
    char text[32] = { 0 };
    int rc;

    if (s_pointCount < 2)
    {
        Lib_Beep(BEEP_TIME_DEF);
        sig_set_status("Please sign first", APP_COLOR_WARN);
        return;
    }

    sig_set_status("Printing...", APP_COLOR_ACCENT);

    /* The UI thread is occupied during printing, force a refresh first */
    lv_refr_now(NULL);

    rc = sig_print_bitmap();

    if (rc == RET_OK)
    {
        Lib_Beep(BEEP_TIME_DEF);
        sig_set_status("Printed", APP_COLOR_SUCCESS);
    }
    else if (rc == RET_ERR_TPH_NO_PAPER)
    {
        sig_set_status("Out of paper", APP_COLOR_WARN);
    }
    else
    {
        snprintf(text, sizeof(text), "Print error %d", rc);
        sig_set_status(text, APP_COLOR_WARN);
    }
}

static void sig_clear_cb(lv_event_t *e)
{
    (void)e;
    sig_clear();
}

static void sig_print_cb(lv_event_t *e)
{
    (void)e;
    sig_print();
}

static lv_obj_t *sig_button(lv_obj_t *parent, lv_coord_t x, const char *text,
                            lv_color_t color, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_t *label = NULL;

    if (btn == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, SIG_CARD_Y + SIG_CARD_H + 44);
    lv_obj_set_size(btn, 216, 60);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
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

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

static void sig_create(app_page_t *page)
{
    s_pointCount = 0;

    s_canvas = app_card(page->body, SIG_CARD_X, SIG_CARD_Y, SIG_CARD_W, SIG_CARD_H);
    if (s_canvas != NULL)
    {
        /* Do not change the signature box background on press while writing */
        lv_obj_set_style_bg_color(s_canvas, APP_COLOR_CARD, LV_STATE_PRESSED);

        lv_obj_add_event_cb(s_canvas, sig_touch_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(s_canvas, sig_touch_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(s_canvas, sig_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    }

    s_hintLabel = lv_label_create(page->body);
    lv_label_set_text(s_hintLabel, LIB_TR("Sign here"));
    lv_obj_set_style_text_color(s_hintLabel, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(s_hintLabel, app_font(20), 0);
    if (s_canvas != NULL)
    {
        lv_obj_align_to(s_hintLabel, s_canvas, LV_ALIGN_CENTER, 0, 0);
    }
    else
    {
        lv_obj_align(s_hintLabel, LV_ALIGN_TOP_MID, 0, SIG_CARD_Y + 190);
    }

    s_statusLabel = lv_label_create(page->body);
    lv_label_set_text(s_statusLabel, LIB_TR("Sign in the box, then press Print"));
    lv_obj_set_style_text_color(s_statusLabel, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(s_statusLabel, app_font(16), 0);
    lv_obj_set_pos(s_statusLabel, SIG_CARD_X + 8, SIG_CARD_Y + SIG_CARD_H + 12);

    sig_button(page->body, SIG_CARD_X, LIB_TR("Clear"), APP_COLOR_SUB, sig_clear_cb);
    sig_button(page->body, SIG_CARD_X + 232, LIB_TR("Print"), APP_COLOR_ACCENT, sig_print_cb);
}

static void sig_destroy(app_page_t *page)
{
    (void)page;

    s_canvas = NULL;
    s_hintLabel = NULL;
    s_statusLabel = NULL;
    s_pointCount = 0;
}

static bool sig_event(app_page_t *page, const app_event_t *ev)
{
    (void)page;

    if (ev->type != APP_EV_KEY)
    {
        return false;
    }

    if (ev->param == KB_ENTER)
    {
        sig_print();
        return true;
    }

    if (ev->param == KB_CLEAR)
    {
        sig_clear();
        return true;
    }

    return false;
}

static app_page_t s_signaturePage =
{
    .id         = "signature",
    .title      = "E-SIGNATURE",
    .on_create  = sig_create,
    .on_event   = sig_event,
    .on_destroy = sig_destroy,
    .user_data  = NULL,
};

app_page_t *page_signature_get(void)
{
    return &s_signaturePage;
}
