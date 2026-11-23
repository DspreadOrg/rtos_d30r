/**********************************************************************************************************
 * File Name     : app_ui.c
 * Description   : D30R theme and common widget factory
 **********************************************************************************************************/

#include "app_ui.h"
#include "app_i18n.h"
#include "LibApi.h"     /* Already includes LibLvgl.h (its include guard has a typo, do not include it twice) */
#include "LibFile.h"
#include "include/extra/libs/freetype/lv_freetype.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/*
 * The SDK's Lib_LvglGetFont() caches FreeType font faces by size and returns the old font on
 * a cache hit, so Lib_LangSet() changing E_FONT_NAME does not refresh it. Persian needs a
 * different glyph set, so we read the TTF ourselves and build a font table cached by (file, size).
 */
#define APP_FONT_CACHE_MAX      24
#define APP_FONT_BLOB_MAX       4
#define APP_FONT_NAME_MAX       32

typedef struct
{
    char            name[APP_FONT_NAME_MAX];
    unsigned short  size;
    lv_font_t      *font;
} app_font_cache_t;

typedef struct
{
    char            name[APP_FONT_NAME_MAX];
    unsigned char  *data;
    unsigned long   size;
} app_font_blob_t;

static app_font_cache_t s_fontCache[APP_FONT_CACHE_MAX];
static app_font_blob_t  s_fontBlobs[APP_FONT_BLOB_MAX];
static unsigned char    s_fontCacheCount = 0;

/* Read the TTF into RAM. FreeType's FT_New_Memory_Face does not copy the data,
   so the buffer must be kept for good and must not be freed. */
static app_font_blob_t *app_font_blob_get(const char *name)
{
    unsigned char i;

    for (i = 0; i < APP_FONT_BLOB_MAX; i++)
    {
        if (s_fontBlobs[i].data != NULL && strcmp(s_fontBlobs[i].name, name) == 0)
        {
            return &s_fontBlobs[i];
        }
    }

    for (i = 0; i < APP_FONT_BLOB_MAX; i++)
    {
        int fd = 0;
        int fileSize = 0;
        int readLen = 0;
        unsigned char *data = NULL;

        if (s_fontBlobs[i].data != NULL)
        {
            continue;
        }

        fileSize = Lib_FileSize((char *)name);
        if (fileSize <= 0)
        {
            Lib_Trace("[FONT] %s not found, size=%d\n", name, fileSize);
            return NULL;
        }

        fd = Lib_FileOpen((char *)name, LFS_O_RDONLY);
        if (fd <= 0)
        {
            Lib_Trace("[FONT] open %s failed, fd=%d\n", name, fd);
            return NULL;
        }

        data = (unsigned char *)malloc((size_t)fileSize);
        if (data == NULL)
        {
            Lib_Trace("[FONT] no memory for %s, size=%d\n", name, fileSize);
            Lib_FileClose(fd);
            return NULL;
        }

        readLen = Lib_FileRead(fd, data, (unsigned long)fileSize);
        Lib_FileClose(fd);

        if (readLen != fileSize)
        {
            Lib_Trace("[FONT] read %s failed, %d/%d\n", name, readLen, fileSize);
            free(data);
            return NULL;
        }

        snprintf(s_fontBlobs[i].name, sizeof(s_fontBlobs[i].name), "%s", name);
        s_fontBlobs[i].data = data;
        s_fontBlobs[i].size = (unsigned long)fileSize;

        return &s_fontBlobs[i];
    }

    return NULL;
}

static lv_font_t *app_font_load(const char *name, unsigned short size)
{
    app_font_blob_t *blob = NULL;
    lv_ft_info_t info;
    unsigned char i;

    for (i = 0; i < s_fontCacheCount; i++)
    {
        if (s_fontCache[i].size == size && strcmp(s_fontCache[i].name, name) == 0)
        {
            return s_fontCache[i].font;
        }
    }

    if (s_fontCacheCount >= APP_FONT_CACHE_MAX)
    {
        return NULL;
    }

    blob = app_font_blob_get(name);
    if (blob == NULL)
    {
        return NULL;
    }

    memset(&info, 0, sizeof(info));
    info.name     = blob->name;
    info.mem      = blob->data;
    info.mem_size = blob->size;
    info.weight   = size;
    info.style    = FT_FONT_STYLE_NORMAL;

    if (!lv_ft_font_init(&info) || info.font == NULL)
    {
        Lib_Trace("[FONT] lv_ft_font_init(%s, %d) failed\n", name, size);
        return NULL;
    }

    Lib_Trace("[FONT] loaded %s, size=%d, font=%p\n", name, size, (void *)info.font);

    snprintf(s_fontCache[s_fontCacheCount].name,
             sizeof(s_fontCache[s_fontCacheCount].name), "%s", name);
    s_fontCache[s_fontCacheCount].size = size;
    s_fontCache[s_fontCacheCount].font = info.font;
    s_fontCacheCount++;

    return info.font;
}

lv_font_t *app_font(unsigned short size)
{
    unsigned char rtl = Lib_LangIsRtl(Lib_LangGetCurrent());
    uint32_t probe = (rtl != 0) ? 0x0641 : 'A';   /* 'ف' or 'A' */
    lv_font_t *font = NULL;
    lv_font_glyph_dsc_t dsc;

    if (rtl != 0)
    {
        /* Persian: prefer the dedicated font, the SDK cache usually holds a Latin font */
        font = app_font_load(APP_FONT_FILE_PERSIAN, size);
        if (font != NULL && lv_font_get_glyph_dsc(font, &dsc, probe, 0))
        {
            return font;
        }

        font = Lib_LvglGetFont(size);
        if (font != NULL && lv_font_get_glyph_dsc(font, &dsc, probe, 0))
        {
            return font;
        }
    }
    else
    {
        /* English/Portuguese: use the SDK's cached Latin font first to avoid reloading */
        font = Lib_LvglGetFont(size);
        if (font != NULL && lv_font_get_glyph_dsc(font, &dsc, probe, 0))
        {
            return font;
        }

        font = app_font_load(APP_FONT_FILE_LATIN, size);
        if (font != NULL && lv_font_get_glyph_dsc(font, &dsc, probe, 0))
        {
            return font;
        }
    }

    return (lv_font_t *)LV_FONT_DEFAULT;
}

lv_obj_t *app_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);

    if (card == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, APP_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_ofs_y(card, 3, 0);
    lv_obj_set_style_shadow_color(card, APP_COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(card, APP_COLOR_ICON_BG, LV_STATE_PRESSED);

    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    return card;
}

lv_obj_t *app_icon_circle(lv_obj_t *parent, lv_coord_t size)
{
    lv_obj_t *circle = lv_obj_create(parent);

    if (circle == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, size, size);
    lv_obj_set_style_radius(circle, size / 2, 0);
    lv_obj_set_style_bg_color(circle, APP_COLOR_ICON_BG, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);

    return circle;
}

lv_obj_t *app_row(lv_obj_t *parent, lv_coord_t y, const char *name)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *nameLabel = NULL;
    lv_obj_t *valueLabel = NULL;

    if (row == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, APP_ROW_X, y);
    lv_obj_set_size(row, APP_ROW_W, APP_ROW_H);
    lv_obj_set_style_bg_color(row, APP_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    nameLabel = lv_label_create(row);
    lv_label_set_text(nameLabel, LIB_TR((name != NULL) ? name : ""));
    lv_obj_set_style_text_color(nameLabel, APP_COLOR_SUB, 0);
    lv_obj_set_style_text_font(nameLabel, app_font(18), 0);
    lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 16, 0);

    valueLabel = lv_label_create(row);
    lv_label_set_text(valueLabel, "-");
    lv_obj_set_width(valueLabel, 260);
    lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(valueLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(valueLabel, APP_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(valueLabel, app_font(18), 0);
    lv_obj_align(valueLabel, LV_ALIGN_RIGHT_MID, -16, 0);

    return valueLabel;
}

void app_ui_init(void)
{
    /* Theme currently uses compile-time constants; override here once color_param is hooked up */
}

/* ---------------- Android-style WiFi fan signal icon ---------------- */

#define APP_WIFI_ICON_ARCS      4

/* Diameter ratio of the four arcs (relative to 28px) */
static const lv_coord_t s_wifiArcScale[APP_WIFI_ICON_ARCS] = { 10, 16, 22, 28 };

unsigned char app_signal_level_from_raw(int raw)
{
    if (raw < 0)    /* dBm (WiFi RSSI) */
    {
        if (raw >= -55)
        {
            return 4;
        }
        if (raw >= -67)
        {
            return 3;
        }
        if (raw >= -79)
        {
            return 2;
        }
        if (raw >= -90)
        {
            return 1;
        }
        return 0;
    }

    if (raw <= 4)   /* Already a 0~4 level */
    {
        return (unsigned char)raw;
    }

    if (raw <= 31)  /* CSQ 0~31 */
    {
        if (raw >= 25)
        {
            return 4;
        }
        if (raw >= 16)
        {
            return 3;
        }
        if (raw >= 9)
        {
            return 2;
        }
        return 1;
    }

    /* 0~100 percentage */
    if (raw >= 75)
    {
        return 4;
    }
    if (raw >= 50)
    {
        return 3;
    }
    if (raw >= 25)
    {
        return 2;
    }
    if (raw > 0)
    {
        return 1;
    }
    return 0;
}

void app_wifi_signal_set_level(lv_obj_t *icon, int level)
{
    int i;

    if (icon == NULL)
    {
        return;
    }

    for (i = 0; i < APP_WIFI_ICON_ARCS; i++)
    {
        lv_obj_t *arc = lv_obj_get_child(icon, i);

        if (arc == NULL)
        {
            continue;
        }

        lv_obj_set_style_arc_opa(arc, (i < level) ? LV_OPA_COVER : LV_OPA_30, LV_PART_INDICATOR);
    }
}

void app_wifi_signal_set_color(lv_obj_t *icon, lv_color_t color)
{
    int i;

    if (icon == NULL)
    {
        return;
    }

    for (i = 0; i < APP_WIFI_ICON_ARCS; i++)
    {
        lv_obj_t *arc = lv_obj_get_child(icon, i);

        if (arc != NULL)
        {
            lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
        }
    }
}

lv_obj_t *app_wifi_signal_icon(lv_obj_t *parent, lv_coord_t size, lv_color_t color)
{
    lv_obj_t *icon = lv_obj_create(parent);
    int i;

    if (icon == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, size, size);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    for (i = 0; i < APP_WIFI_ICON_ARCS; i++)
    {
        /* Arc center fixed at the bottom edge midpoint, draw only the upward 225°~315° fan */
        lv_coord_t arcSize = (size * s_wifiArcScale[i]) / 28;
        lv_coord_t width = (i == 0) ? ((size * 6 + 14) / 28) : ((size * 3 + 14) / 28);
        lv_obj_t *arc = lv_arc_create(icon);

        if (arc == NULL)
        {
            continue;
        }

        lv_obj_remove_style_all(arc);
        lv_obj_set_size(arc, arcSize, arcSize);
        lv_obj_set_pos(arc, size / 2 - arcSize / 2, size - arcSize / 2);

        lv_arc_set_rotation(arc, 0);
        lv_arc_set_bg_angles(arc, 225, 315);
        lv_arc_set_angles(arc, 225, 315);

        /* Draw the fan with INDICATOR only, keep MAIN (background arc) transparent, otherwise it looks always on */
        lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, LV_OPA_30, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_SCROLLABLE);
    }

    return icon;
}

/* ================= Legacy framework compatibility layer (original Driver/Middle code removed) ================= */

/* ---- Icon font (was api_lvgl.c) ---- */
static const uint8_t custom_back_glyph_bitmap[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x1F,0xFF,0xF8,0x1F,0xFF,0xF8,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0xFF,0xF8,
    0x1F,0xFF,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x1F,0xFF,0xF8,0x1F,0xFF,0xF8,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xFF,0xF0,0x1F,0xFF,0xF8,
    0x38,0x00,0x1C,0x30,0x00,0x0C,0x30,0x00,0x0C,0x30,0x00,0x0C,
    0x30,0x00,0x0C,0x30,0x00,0x0C,0x30,0x00,0x0C,0x30,0x00,0x0C,
    0x30,0x00,0x0C,0x30,0x00,0x0C,0x30,0x00,0x0C,0x30,0x00,0x0C,
    0x30,0x00,0x0C,0x30,0x00,0x0C,0x30,0x00,0x0C,0x38,0x00,0x1C,
    0x1F,0xFF,0xF8,0x0F,0xFF,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xC0,0x00,0x03,0x80,
    0x00,0x07,0x00,0x00,0x0E,0x00,0x00,0x1C,0x00,0x00,0x38,0x00,
    0x00,0x70,0x00,0x00,0xE0,0x00,0x01,0xC0,0x00,0x01,0x80,0x00,
    0x01,0x80,0x00,0x01,0xC0,0x00,0x00,0xE0,0x00,0x00,0x70,0x00,
    0x00,0x38,0x00,0x00,0x1C,0x00,0x00,0x0E,0x00,0x00,0x07,0x00,
    0x00,0x03,0x80,0x00,0x01,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,

    0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x03,0x99,0xC0,
    0x07,0x99,0xE0,0x0E,0x18,0x70,0x1C,0x18,0x38,0x38,0x18,0x1C,
    0x30,0x18,0x0C,0x70,0x18,0x0E,0x60,0x18,0x06,0x60,0x18,0x06,
    0x60,0x18,0x06,0x60,0x00,0x06,0x60,0x00,0x06,0x60,0x00,0x06,
    0x60,0x00,0x06,0x70,0x00,0x0E,0x38,0x00,0x1C,0x1C,0x00,0x38,
    0x0E,0x00,0x70,0x07,0x81,0xE0,0x03,0xFF,0xC0,0x00,0xFF,0x00,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static const lv_font_fmt_txt_glyph_dsc_t custom_glyph_dsc[] =
{
    { 0,0,0,0,0,0},
    { .bitmap_index=0,  .adv_w=24*16, .box_w=24, .box_h=24 },
    { .bitmap_index=72, .adv_w=24*16, .box_w=24, .box_h=24 },
    { .bitmap_index=144,.adv_w=24*16, .box_w=24, .box_h=24 },
    { .bitmap_index=216,.adv_w=24*16, .box_w=24, .box_h=24 },
    { .bitmap_index=288,.adv_w=24*16, .box_w=24, .box_h=24 },
    { .bitmap_index=360,.adv_w=24*16, .box_w=24, .box_h=24 },
};

static const lv_font_fmt_txt_cmap_t custom_cmaps[] =
{
    {
        .range_start       = 0xE000,
        .range_length      = 6,
        .glyph_id_start    = 1,
        .unicode_list      = NULL,
        .glyph_id_ofs_list = NULL,
        .list_length       = 0,
        .type              = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    }
};

static const lv_font_fmt_txt_dsc_t custom_font_dsc =
{
    .glyph_bitmap  = custom_back_glyph_bitmap,
    .glyph_dsc     = custom_glyph_dsc,
    .cmaps         = custom_cmaps,
    .kern_dsc      = NULL,
    .kern_scale    = 0,
    .cmap_num      = 1,
    .bpp           = 1,
    .kern_classes  = 0,
    .bitmap_format = 0,
};

lv_font_t custom_back_font =
{
    .get_glyph_dsc    = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height      = 24,
    .base_line        = 0,
    .dsc              = &custom_font_dsc,
};

/* ---- Page container (was api_lvgl.c) ---- */
static lv_obj_t *s_pageBody = NULL;

lv_obj_t *LvUiGetPageBody(void)
{
    lv_obj_t *scr = NULL;
    if (s_pageBody != NULL)
    {
        return s_pageBody;
    }

    scr = lv_scr_act();
    if (scr == NULL)
    {
        return NULL;
    }

    s_pageBody = lv_obj_create(scr);
    if (s_pageBody == NULL)
    {
        return NULL;
    }

    lv_obj_remove_style_all(s_pageBody);
    lv_obj_set_pos(s_pageBody, 0, APP_STATUS_BAR_H);
    lv_obj_set_size(s_pageBody, APP_SCREEN_W, APP_SCREEN_H - APP_STATUS_BAR_H - APP_NAV_BAR_H);
    lv_obj_set_style_bg_color(s_pageBody, APP_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_pageBody, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_pageBody, LV_OBJ_FLAG_SCROLLABLE);

    return s_pageBody;
}

void LvUilCls(void)
{
    if (s_pageBody != NULL)
    {
        lv_obj_clean(s_pageBody);
    }
}

/* ---- Cancel request (was UiHal.c) ---- */
static unsigned char s_cancelRequest = 0;

void UIHalRequestCancel(void)
{
    s_cancelRequest = 1;
}

void UIHalClearCancel(void)
{
    s_cancelRequest = 0;
}

int UIHalCancelTakeRequest(void)
{
    if (s_cancelRequest)
    {
        s_cancelRequest = 0;
        return 1;
    }
    return 0;
}


/* ---- Logging (was Common.c) ---- */
extern void TRACE(const char *pcFormat, ...);

void ApiLibTrace(const char *fmt, ...)
{
    char acBuf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(acBuf, sizeof(acBuf), fmt, ap);
    va_end(ap);

    TRACE("%s", acBuf);
}

void ApiLibTraceBuf(uint8_t *pBuf, uint16_t len)
{
    TRACE("TRACEBUF len=%d", (int)len);
    (void)pBuf;
}

/* ---- Server selection and TCP communication (was Communication.c) ---- */
#define SINGLE_RECV_LEN             1024
#define NET_MAX_INIT_TIME           (180*1000)
#define NET_MAX_TCPLINK_TIME        (15*1000)

static EN_SERVER_OPT s_serverOpt = SERVER_OPT_TMS_MAIN;

void SetServerOpt(EN_SERVER_OPT serverOpt)
{
    s_serverOpt = serverOpt;
}

EN_SERVER_OPT GetServerOpt(void)
{
    return s_serverOpt;
}
