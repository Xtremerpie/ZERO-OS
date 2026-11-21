/* ZERO SDK - 2D Drawing Interface & Color Palette (Milestone 6.0.0 ZERO Ecosystem).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ZERO_COLOR_BLACK        0x08080Au
#define ZERO_COLOR_DARK_BASE    0x121216u
#define ZERO_COLOR_PANEL        0x1A1A20u
#define ZERO_COLOR_PANEL_ALT    0x22222Au
#define ZERO_COLOR_BORDER       0x2E2E3Au
#define ZERO_COLOR_GOLD         0xD4AF37u
#define ZERO_COLOR_CRIMSON      0xC41230u
#define ZERO_COLOR_WHITE        0xF2F2F4u
#define ZERO_COLOR_MUTED        0x8A8A96u
#define ZERO_COLOR_SUCCESS      0x00D26Au
#define ZERO_COLOR_WARNING      0xF5A623u

typedef struct zero_surface {
    uint32_t *pixels;
    int32_t   width;
    int32_t   height;
    int32_t   pitch;
} zero_surface_t;

static inline uint32_t zero_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline void zero_draw_pixel(zero_surface_t *s, int32_t x, int32_t y, uint32_t col)
{
    if (!s || !s->pixels || x < 0 || x >= s->width || y < 0 || y >= s->height) return;
    s->pixels[y * (s->pitch / 4) + x] = col;
}

static inline void zero_draw_fill_rect(zero_surface_t *s, int32_t rx, int32_t ry, int32_t rw, int32_t rh, uint32_t col)
{
    if (!s || !s->pixels || rw <= 0 || rh <= 0) return;
    for (int32_t y = ry; y < ry + rh; y++) {
        if (y < 0 || y >= s->height) continue;
        for (int32_t x = rx; x < rx + rw; x++) {
            if (x < 0 || x >= s->width) continue;
            s->pixels[y * (s->pitch / 4) + x] = col;
        }
    }
}
