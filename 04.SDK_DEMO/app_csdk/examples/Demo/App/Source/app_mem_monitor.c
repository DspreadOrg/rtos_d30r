/**********************************************************************************************************
 * File Name     : app_mem_monitor.c
 * Description   : Memory status floating panel
 *
 *   Displayed content:
 *     DISP  : LVGL full-screen display buffer (480x854x2, fixed at compile time)
 *     MIRR  : Cast mirror buffer (allocated only when casting starts)
 *     PAGE  : Page stack depth / limit
 *     UP    : System uptime
 *
 *   Tapping the panel calls Lib_PrintfHeapInfo() to dump detailed SDK heap info to the serial log
 **********************************************************************************************************/

#include "app_mem_monitor.h"
#include "app_ui.h"
#include "app_page.h"
#include "LibApi.h"

#include <stdio.h>

#define MM_W                    264
#define MM_H                    160
#define MM_LINE_COUNT           5

/* Heap statistics interface exported by the SDK */
extern void Lib_HeapDumpStat(void **heapStart, uint32_t *size, uint32_t *avail, uint32_t *maxb);

static lv_obj_t *s_panel = NULL;
static lv_obj_t *s_lines[MM_LINE_COUNT];
static lv_timer_t *s_timer = NULL;

static bool mm_get_heap(unsigned long *total, unsigned long *avail, unsigned long *maxblk)
{
    void *heapStart = NULL;
    uint32_t size, availBytes, maxb;

    Lib_Trace("APP:mm_get_heap() Lib_HeapDumpStat");
    Lib_HeapDumpStat(&heapStart, &size, &availBytes, &maxb);

    *total  = (unsigned long)size;
    *avail  = (unsigned long)availBytes;
    *maxblk = (unsigned long)maxb;

    return true;
}

static void mm_refresh(void)
{
    char acBuf[48] = { 0 };
    unsigned long dispBytes = (unsigned long)APP_SCREEN_W * APP_SCREEN_H * 2UL;
    unsigned long total = 0;
    unsigned long avail = 0;
    unsigned long maxblk = 0;
    unsigned long ms;
    unsigned long sec;

    if (s_panel == NULL)
    {
        return;
    }

    if (mm_get_heap(&total, &avail, &maxblk))
    {
        snprintf(acBuf, sizeof(acBuf), "HEAP %luK FREE %luK", total / 1024UL, avail / 1024UL);
        lv_label_set_text(s_lines[0], acBuf);

        snprintf(acBuf, sizeof(acBuf), "USED %luK MAX %luK",
                 (total - avail) / 1024UL, maxblk / 1024UL);
        lv_label_set_text(s_lines[1], acBuf);
    }
    else
    {
        lv_label_set_text(s_lines[0], "HEAP n/a");
        lv_label_set_text(s_lines[1], "need osiMemPoolStat");
    }

    snprintf(acBuf, sizeof(acBuf), "DISP %luK",
             dispBytes / 1024UL);
    lv_label_set_text(s_lines[2], acBuf);

    ms = Lib_SysGetTimer();
    sec = ms / 1000UL;
    snprintf(acBuf, sizeof(acBuf), "PAGE %d/%d  UP %02lu:%02lu:%02lu",
             app_page_depth(), APP_PAGE_MAX_DEPTH,
             sec / 3600UL, (sec / 60UL) % 60UL, sec % 60UL);
    lv_label_set_text(s_lines[3], acBuf);

    lv_label_set_text(s_lines[4], "TAP = DUMP HEAP LOG");
}

static void mm_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    mm_refresh();
}

static void mm_panel_clicked_cb(lv_event_t *e)
{
    (void)e;

    /* Detailed info can only be printed to the serial log */
    Lib_PrintfHeapInfo();
    lv_label_set_text(s_lines[4], "HEAP DUMPED");
}

static void mm_close_clicked_cb(lv_event_t *e)
{
    (void)e;

    app_mem_monitor_toggle();
}

static void mm_build(void)
{
    lv_obj_t *closeBtn = NULL;
    int i;

    s_panel = lv_obj_create(lv_layer_top());
    if (s_panel == NULL)
    {
        return;
    }

    lv_obj_remove_style_all(s_panel);
    lv_obj_set_size(s_panel, MM_W, MM_H);
    lv_obj_align(s_panel, LV_ALIGN_TOP_RIGHT, -10, 34);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x10263D), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_panel, 10, 0);
    lv_obj_set_style_border_width(s_panel, 1, 0);
    lv_obj_set_style_border_color(s_panel, APP_COLOR_ACCENT_2, 0);
    lv_obj_set_style_pad_all(s_panel, 10, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_panel, mm_panel_clicked_cb, LV_EVENT_CLICKED, NULL);

    closeBtn = lv_label_create(s_panel);
    lv_label_set_text(closeBtn, "X");
    lv_obj_set_style_text_color(closeBtn, lv_color_white(), 0);
    lv_obj_set_style_text_font(closeBtn, app_font(14), 0);
    lv_obj_align(closeBtn, LV_ALIGN_TOP_RIGHT, 0, -2);
    lv_obj_add_flag(closeBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(closeBtn, mm_close_clicked_cb, LV_EVENT_CLICKED, NULL);

    for (i = 0; i < MM_LINE_COUNT; i++)
    {
        s_lines[i] = lv_label_create(s_panel);
        lv_label_set_text(s_lines[i], "");
        lv_obj_set_style_text_color(s_lines[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(s_lines[i], app_font(14), 0);
        lv_obj_align(s_lines[i], LV_ALIGN_TOP_LEFT, 2, 18 + i * 26);
    }

    s_timer = lv_timer_create(mm_timer_cb, 1000, NULL);

    mm_refresh();
}

static void mm_destroy(void)
{
    if (s_timer != NULL)
    {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    if (s_panel != NULL)
    {
        lv_obj_del(s_panel);
        s_panel = NULL;
    }

    for (int i = 0; i < MM_LINE_COUNT; i++)
    {
        s_lines[i] = NULL;
    }
}

void app_mem_monitor_toggle(void)
{
    if (s_panel != NULL)
    {
        mm_destroy();
        return;
    }

    mm_build();
}

bool app_mem_monitor_is_open(void)
{
    return (s_panel != NULL);
}
