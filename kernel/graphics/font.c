/* ZERO KERNEL - Typography & Native Font Rendering Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements 8x8 font rasterization with integer scaling, UI icon glyphs, text measurement,
 * and alignment helpers.
 */
#include "graphics/font.h"
#include "graphics/draw.h"
#include "font8x8/font8x8_basic.h"
#include "lib/string.h"

/* Custom UI Icon Glyphs (8x8 bitmaps) */
static const uint8_t custom_icons[11][8] = {
    /* 0x80: GLYPH_ICON_MINIMIZE */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00 },
    /* 0x81: GLYPH_ICON_MAXIMIZE */
    { 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00 },
    /* 0x82: GLYPH_ICON_CLOSE */
    { 0xC3, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0xC3 },
    /* 0x83: GLYPH_ICON_FOLDER */
    { 0x1C, 0x3E, 0x7F, 0x41, 0x41, 0x41, 0x7F, 0x00 },
    /* 0x84: GLYPH_ICON_FILE */
    { 0x3C, 0x24, 0x28, 0x30, 0x20, 0x20, 0x3E, 0x00 },
    /* 0x85: GLYPH_ICON_TERMINAL */
    { 0x00, 0x04, 0x08, 0x10, 0x08, 0x04, 0x70, 0x00 },
    /* 0x86: GLYPH_ICON_GEAR */
    { 0x24, 0x7E, 0xDB, 0xC3, 0xC3, 0xDB, 0x7E, 0x24 },
    /* 0x87: GLYPH_ICON_CPU */
    { 0x5A, 0x7E, 0x5A, 0x5A, 0x5A, 0x5A, 0x7E, 0x5A },
    /* 0x88: GLYPH_ICON_DISK */
    { 0x7E, 0x81, 0xBD, 0x81, 0x7E, 0x81, 0xBD, 0x7E },
    /* 0x89: GLYPH_ICON_NET */
    { 0x00, 0x3C, 0x42, 0x18, 0x24, 0x00, 0x18, 0x00 },
    /* 0x8A: GLYPH_ICON_CHECK */
    { 0x00, 0x02, 0x06, 0x0C, 0x98, 0x70, 0x20, 0x00 }
};

static const uint8_t *get_glyph(uint8_t ch)
{
    if (ch >= 0x80 && ch <= 0x8A) {
        return custom_icons[ch - 0x80];
    }
    if (ch < 128) {
        return (const uint8_t *)font8x8_basic[ch];
    }
    /* Fallback to question mark '?' */
    return (const uint8_t *)font8x8_basic['?'];
}

int32_t font_get_string_width(const char *text, uint32_t scale)
{
    if (!text || scale == 0) return 0;
    return (int32_t)(strlen(text) * FONT_BASE_WIDTH * scale);
}

int32_t font_get_string_height(uint32_t scale)
{
    if (scale == 0) scale = 1;
    return (int32_t)(FONT_BASE_HEIGHT * scale);
}

void font_draw_char(framebuffer_t *fb, int32_t x, int32_t y, uint8_t ch, uint32_t scale, uint32_t color)
{
    if (!fb || !fb->initialized || scale == 0) return;

    const uint8_t *bitmap = get_glyph(ch);

    for (int32_t row = 0; row < 8; row++) {
        uint8_t bits = bitmap[row];
        for (int32_t col = 0; col < 8; col++) {
            if (bits & (1u << col)) {
                if (scale == 1) {
                    draw_pixel(fb, x + col, y + row, color);
                } else {
                    draw_fill_rect(fb, x + (col * (int32_t)scale), y + (row * (int32_t)scale),
                                   (int32_t)scale, (int32_t)scale, color);
                }
            }
        }
    }
}

int32_t font_draw_string(framebuffer_t *fb, int32_t x, int32_t y, const char *text, uint32_t scale, uint32_t color)
{
    if (!fb || !text) return x;
    if (scale == 0) scale = 1;

    int32_t cur_x = x;
    int32_t cur_y = y;
    int32_t char_w = (int32_t)(FONT_BASE_WIDTH * scale);
    int32_t char_h = (int32_t)(FONT_BASE_HEIGHT * scale);

    while (*text) {
        char c = *text++;
        if (c == '\n') {
            cur_x = x;
            cur_y += char_h + (int32_t)(2 * scale);
            continue;
        }
        if (c == '\r') continue;

        font_draw_char(fb, cur_x, cur_y, (uint8_t)c, scale, color);
        cur_x += char_w;
    }

    return cur_x;
}

int32_t font_draw_string_clipped(framebuffer_t *fb, int32_t x, int32_t y, const char *text, uint32_t scale, uint32_t color, const rect_t *clip)
{
    if (!fb || !text) return x;

    rect_t old_clip = fb->clip;
    if (clip) {
        fb_set_clip(fb, clip->x, clip->y, clip->w, clip->h);
    }

    int32_t res_x = font_draw_string(fb, x, y, text, scale, color);

    if (clip) {
        fb->clip = old_clip;
    }
    return res_x;
}

void font_draw_string_centered(framebuffer_t *fb, int32_t box_x, int32_t box_y, int32_t box_w, int32_t box_h,
                               const char *text, uint32_t scale, uint32_t color)
{
    if (!fb || !text) return;
    if (scale == 0) scale = 1;

    int32_t str_w = font_get_string_width(text, scale);
    int32_t str_h = font_get_string_height(scale);

    int32_t start_x = box_x + (box_w - str_w) / 2;
    int32_t start_y = box_y + (box_h - str_h) / 2;

    font_draw_string(fb, start_x, start_y, text, scale, color);
}
