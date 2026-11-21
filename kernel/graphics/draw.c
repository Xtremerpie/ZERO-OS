/* ZERO KERNEL - 2D Drawing Primitives & Vector Engine Implementation (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements clipping, line rasterization, filled rectangles, rounded corners,
 * circles, linear gradients, and alpha blending.
 */
#include "graphics/draw.h"
#include "lib/string.h"

static inline int32_t i_abs(int32_t v) { return (v < 0) ? -v : v; }
static inline int32_t i_min(int32_t a, int32_t b) { return (a < b) ? a : b; }
static inline int32_t i_max(int32_t a, int32_t b) { return (a > b) ? a : b; }

void draw_pixel(framebuffer_t *fb, int32_t x, int32_t y, uint32_t color)
{
    if (!fb || !fb->initialized) return;

    /* Enforce clipping boundaries */
    if (x < fb->clip.x || x >= (fb->clip.x + fb->clip.w) ||
        y < fb->clip.y || y >= (fb->clip.y + fb->clip.h)) {
        return;
    }

    if ((uint32_t)x >= fb->width || (uint32_t)y >= fb->height) {
        return;
    }

    fb->back_buffer[(uint32_t)y * fb->pixels_per_pitch + (uint32_t)x] = fb_pack_color(fb, color);
}

void draw_blend_pixel(framebuffer_t *fb, int32_t x, int32_t y, uint32_t color, uint8_t alpha)
{
    if (!fb || !fb->initialized || alpha == 0) return;
    if (alpha == 255) {
        draw_pixel(fb, x, y, color);
        return;
    }

    if (x < fb->clip.x || x >= (fb->clip.x + fb->clip.w) ||
        y < fb->clip.y || y >= (fb->clip.y + fb->clip.h)) {
        return;
    }

    if ((uint32_t)x >= fb->width || (uint32_t)y >= fb->height) {
        return;
    }

    uint32_t offset = (uint32_t)y * fb->pixels_per_pitch + (uint32_t)x;
    uint32_t bg_packed = fb->back_buffer[offset];

    /* Unpack background RGB */
    uint32_t bg_r = (bg_packed >> fb->red_shift) & 0xFFu;
    uint32_t bg_g = (bg_packed >> fb->green_shift) & 0xFFu;
    uint32_t bg_b = (bg_packed >> fb->blue_shift) & 0xFFu;

    /* Unpack foreground RGB */
    uint32_t fg_r = color_get_r(color);
    uint32_t fg_g = color_get_g(color);
    uint32_t fg_b = color_get_b(color);

    /* Alpha blend: out = (fg * a + bg * (255 - a)) / 255 */
    uint32_t inv_a = 255u - (uint32_t)alpha;
    uint32_t out_r = ((fg_r * (uint32_t)alpha) + (bg_r * inv_a)) / 255u;
    uint32_t out_g = ((fg_g * (uint32_t)alpha) + (bg_g * inv_a)) / 255u;
    uint32_t out_b = ((fg_b * (uint32_t)alpha) + (bg_b * inv_a)) / 255u;

    uint32_t blended_rgb = color_rgb((uint8_t)out_r, (uint8_t)out_g, (uint8_t)out_b);
    fb->back_buffer[offset] = fb_pack_color(fb, blended_rgb);
}

void draw_line(framebuffer_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (!fb || !fb->initialized) return;

    int32_t dx = i_abs(x1 - x0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = -i_abs(y1 - y0);
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    int32_t min_x = i_min(x0, x1);
    int32_t min_y = i_min(y0, y1);
    int32_t max_x = i_max(x0, x1);
    int32_t max_y = i_max(y0, y1);

    while (1) {
        draw_pixel(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }

    fb_mark_damage(fb, min_x, min_y, (max_x - min_x) + 1, (max_y - min_y) + 1);
}

void draw_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
    if (!fb || w <= 0 || h <= 0) return;

    /* Horizontal edges */
    for (int32_t i = 0; i < w; i++) {
        draw_pixel(fb, x + i, y, color);
        draw_pixel(fb, x + i, y + h - 1, color);
    }
    /* Vertical edges */
    for (int32_t i = 1; i < h - 1; i++) {
        draw_pixel(fb, x, y + i, color);
        draw_pixel(fb, x + w - 1, y + i, color);
    }

    fb_mark_damage(fb, x, y, w, h);
}

void draw_fill_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
    if (!fb || !fb->initialized || w <= 0 || h <= 0) return;

    /* Clip against active clip rect */
    int32_t cx0 = i_max(x, fb->clip.x);
    int32_t cy0 = i_max(y, fb->clip.y);
    int32_t cx1 = i_min(x + w, fb->clip.x + fb->clip.w);
    int32_t cy1 = i_min(y + h, fb->clip.y + fb->clip.h);

    /* Clip against physical screen bounds */
    cx0 = i_max(cx0, 0);
    cy0 = i_max(cy0, 0);
    cx1 = i_min(cx1, (int32_t)fb->width);
    cy1 = i_min(cy1, (int32_t)fb->height);

    if (cx1 <= cx0 || cy1 <= cy0) return;

    uint32_t packed = fb_pack_color(fb, color);
    uint32_t fill_w = (uint32_t)(cx1 - cx0);

    for (int32_t cy = cy0; cy < cy1; cy++) {
        uint32_t *row = &fb->back_buffer[(uint32_t)cy * fb->pixels_per_pitch + (uint32_t)cx0];
        for (uint32_t i = 0; i < fill_w; i++) {
            row[i] = packed;
        }
    }

    fb_mark_damage(fb, cx0, cy0, cx1 - cx0, cy1 - cy0);
}

void draw_circle(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t radius, uint32_t color)
{
    if (!fb || radius <= 0) return;

    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 3 - 2 * radius;

    while (y >= x) {
        draw_pixel(fb, cx + x, cy + y, color);
        draw_pixel(fb, cx - x, cy + y, color);
        draw_pixel(fb, cx + x, cy - y, color);
        draw_pixel(fb, cx - x, cy - y, color);
        draw_pixel(fb, cx + y, cy + x, color);
        draw_pixel(fb, cx - y, cy + x, color);
        draw_pixel(fb, cx + y, cy - x, color);
        draw_pixel(fb, cx - y, cy - x, color);

        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    fb_mark_damage(fb, cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1);
}

void draw_fill_circle(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t radius, uint32_t color)
{
    if (!fb || radius <= 0) return;

    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 3 - 2 * radius;

    while (y >= x) {
        draw_fill_rect(fb, cx - x, cy - y, x * 2 + 1, 1, color);
        draw_fill_rect(fb, cx - x, cy + y, x * 2 + 1, 1, color);
        draw_fill_rect(fb, cx - y, cy - x, y * 2 + 1, 1, color);
        draw_fill_rect(fb, cx - y, cy + x, y * 2 + 1, 1, color);

        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    fb_mark_damage(fb, cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1);
}

void draw_rounded_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color)
{
    if (!fb || w <= 0 || h <= 0) return;
    if (r <= 0) { draw_rect(fb, x, y, w, h, color); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    /* Top & bottom straight edges */
    for (int32_t i = x + r; i < x + w - r; i++) {
        draw_pixel(fb, i, y, color);
        draw_pixel(fb, i, y + h - 1, color);
    }
    /* Left & right straight edges */
    for (int32_t i = y + r; i < y + h - r; i++) {
        draw_pixel(fb, x, i, color);
        draw_pixel(fb, x + w - 1, i, color);
    }

    /* 4 rounded corners using circle algorithm */
    int32_t cx0 = x + r, cx1 = x + w - 1 - r;
    int32_t cy0 = y + r, cy1 = y + h - 1 - r;
    int32_t px = 0, py = r, d = 3 - 2 * r;

    while (py >= px) {
        draw_pixel(fb, cx1 + px, cy1 + py, color);
        draw_pixel(fb, cx0 - px, cy1 + py, color);
        draw_pixel(fb, cx1 + px, cy0 - py, color);
        draw_pixel(fb, cx0 - px, cy0 - py, color);

        draw_pixel(fb, cx1 + py, cy1 + px, color);
        draw_pixel(fb, cx0 - py, cy1 + px, color);
        draw_pixel(fb, cx1 + py, cy0 - px, color);
        draw_pixel(fb, cx0 - py, cy0 - px, color);

        if (d < 0) d = d + 4 * px + 6;
        else { d = d + 4 * (px - py) + 10; py--; }
        px++;
    }

    fb_mark_damage(fb, x, y, w, h);
}

void draw_fill_rounded_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color)
{
    if (!fb || w <= 0 || h <= 0) return;
    if (r <= 0) { draw_fill_rect(fb, x, y, w, h, color); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    /* Fill main center vertical block */
    draw_fill_rect(fb, x, y + r, w, h - 2 * r, color);

    /* Fill top and bottom middle strips */
    draw_fill_rect(fb, x + r, y, w - 2 * r, r, color);
    draw_fill_rect(fb, x + r, y + h - r, w - 2 * r, r, color);

    /* Fill corner arcs */
    int32_t cx0 = x + r, cx1 = x + w - 1 - r;
    int32_t cy0 = y + r, cy1 = y + h - 1 - r;
    int32_t px = 0, py = r, d = 3 - 2 * r;

    while (py >= px) {
        draw_fill_rect(fb, cx0 - py, cy0 - px, (cx1 + py) - (cx0 - py) + 1, 1, color);
        draw_fill_rect(fb, cx0 - px, cy0 - py, (cx1 + px) - (cx0 - px) + 1, 1, color);
        draw_fill_rect(fb, cx0 - px, cy1 + py, (cx1 + px) - (cx0 - px) + 1, 1, color);
        draw_fill_rect(fb, cx0 - py, cy1 + px, (cx1 + py) - (cx0 - py) + 1, 1, color);

        if (d < 0) d = d + 4 * px + 6;
        else { d = d + 4 * (px - py) + 10; py--; }
        px++;
    }

    fb_mark_damage(fb, x, y, w, h);
}

void draw_gradient_v(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color_top, uint32_t color_bottom)
{
    if (!fb || w <= 0 || h <= 0) return;

    int32_t r0 = (int32_t)color_get_r(color_top);
    int32_t g0 = (int32_t)color_get_g(color_top);
    int32_t b0 = (int32_t)color_get_b(color_top);

    int32_t r1 = (int32_t)color_get_r(color_bottom);
    int32_t g1 = (int32_t)color_get_g(color_bottom);
    int32_t b1 = (int32_t)color_get_b(color_bottom);

    for (int32_t i = 0; i < h; i++) {
        int32_t cr = r0 + ((r1 - r0) * i) / h;
        int32_t cg = g0 + ((g1 - g0) * i) / h;
        int32_t cb = b0 + ((b1 - b0) * i) / h;
        uint32_t line_color = color_rgb((uint8_t)cr, (uint8_t)cg, (uint8_t)cb);
        draw_fill_rect(fb, x, y + i, w, 1, line_color);
    }

    fb_mark_damage(fb, x, y, w, h);
}

void draw_image(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t *pixels, bool has_alpha)
{
    if (!fb || !pixels || w <= 0 || h <= 0) return;

    for (int32_t row = 0; row < h; row++) {
        for (int32_t col = 0; col < w; col++) {
            uint32_t pix = pixels[row * w + col];
            if (has_alpha) {
                uint8_t a = (uint8_t)((pix >> 24) & 0xFFu);
                if (a > 0) {
                    draw_blend_pixel(fb, x + col, y + row, pix & 0x00FFFFFFu, a);
                }
            } else {
                draw_pixel(fb, x + col, y + row, pix & 0x00FFFFFFu);
            }
        }
    }

    fb_mark_damage(fb, x, y, w, h);
}
