/* ZERO KERNEL - early framebuffer console.
 *
 * Draws with the vendored public-domain 8x8 font (bit 0 = leftmost pixel).
 * Writes straight to the Limine-provided framebuffer; no double buffering,
 * no acceleration. When the log area fills it is cleared and restarts at the
 * top (the serial log is always the complete record). */

#include <stdbool.h>
#include <stdint.h>
#include "zero/fbcon.h"
#include "zero/string.h"
#include "font8x8/font8x8_basic.h"

#define BG_COLOUR 0x0A0A0A

static struct {
    uint8_t *addr;
    uint32_t width, height, pitch;
    uint8_t red_shift, green_shift, blue_shift;
    bool ok;
} fb;

static struct {
    uint32_t top, scale;
    uint32_t cx, cy;
    uint32_t fg;
} con = { .fg = 0xE8E8E8, .scale = 1 };

bool fbcon_init(const struct limine_framebuffer *f)
{
    if (!f || f->bpp != 32 || f->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        f->red_mask_size != 8 || f->green_mask_size != 8 || f->blue_mask_size != 8)
        return false;

    /* Pixels are written as uint32_t: the base and every row must be 4-byte aligned. */
    if (!f->address || ((uintptr_t)f->address & 3) || (f->pitch & 3) || f->pitch < f->width * 4)
        return false;

    fb.addr = f->address;
    fb.width = (uint32_t)f->width;
    fb.height = (uint32_t)f->height;
    fb.pitch = (uint32_t)f->pitch;
    fb.red_shift = f->red_mask_shift;
    fb.green_shift = f->green_mask_shift;
    fb.blue_shift = f->blue_mask_shift;
    fb.ok = true;
    return true;
}

bool fbcon_available(void) { return fb.ok; }
uint32_t fbcon_width(void) { return fb.width; }
uint32_t fbcon_height(void) { return fb.height; }

static inline uint32_t pack(uint32_t rgb)
{
    return (((rgb >> 16) & 0xFF) << fb.red_shift) |
           (((rgb >> 8) & 0xFF) << fb.green_shift) |
           ((rgb & 0xFF) << fb.blue_shift);
}

void fbcon_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb)
{
    if (!fb.ok || x >= fb.width || y >= fb.height)
        return;
    /* Overflow-safe clipping: compare against the remaining space, never add x + w. */
    if (w > fb.width - x)
        w = fb.width - x;
    if (h > fb.height - y)
        h = fb.height - y;

    uint32_t px = pack(rgb);
    for (uint32_t row = 0; row < h; row++) {
        uint32_t *p = (uint32_t *)(fb.addr + (uint64_t)(y + row) * fb.pitch) + x;
        for (uint32_t col = 0; col < w; col++)
            p[col] = px;
    }
}

void fbcon_clear(uint32_t rgb)
{
    fbcon_fill_rect(0, 0, fb.width, fb.height, rgb);
}

static void draw_glyph(uint32_t x, uint32_t y, unsigned char ch, uint32_t scale, uint32_t rgb)
{
    if (ch >= 128)
        ch = '?';
    const unsigned char *rows = font8x8_basic[ch];
    for (uint32_t gy = 0; gy < 8; gy++) {
        for (uint32_t gx = 0; gx < 8; gx++) {
            if (rows[gy] & (1u << gx))
                fbcon_fill_rect(x + gx * scale, y + gy * scale, scale, scale, rgb);
        }
    }
}

uint32_t fbcon_draw_string(uint32_t x, uint32_t y, const char *s, uint32_t scale, uint32_t rgb)
{
    if (!fb.ok)
        return x;
    for (; *s; s++) {
        draw_glyph(x, y, (unsigned char)*s, scale, rgb);
        x += 8 * scale;
    }
    return x;
}

void fbcon_console_begin(uint32_t top_px, uint32_t scale)
{
    con.top = top_px;
    con.scale = scale ? scale : 1;
    con.cx = 0;
    con.cy = top_px;
    fbcon_fill_rect(0, top_px, fb.width, fb.height - top_px, BG_COLOUR);
}

void fbcon_set_fg(uint32_t rgb)
{
    con.fg = rgb;
}

static void newline(void)
{
    uint32_t line_h = 8 * con.scale + 2 * con.scale;
    con.cx = 0;
    con.cy += line_h;
    if (con.cy + line_h > fb.height) {
        fbcon_fill_rect(0, con.top, fb.width, fb.height - con.top, BG_COLOUR);
        con.cy = con.top;
    }
}

void fbcon_putc(char c)
{
    /* Before fbcon_console_begin() there is no log area: stay silent. */
    if (!fb.ok || con.top == 0)
        return;

    if (c == '\n') {
        newline();
        return;
    }
    if (c == '\r')
        return;

    uint32_t cell_w = 8 * con.scale;
    if (con.cx + cell_w + cell_w / 2 > fb.width)
        newline();

    draw_glyph(con.cx + cell_w / 2, con.cy, (unsigned char)c, con.scale, con.fg);
    con.cx += cell_w;
}
