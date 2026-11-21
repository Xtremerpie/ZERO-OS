/* ZERO KERNEL - Framebuffer Architecture & Double Buffer (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements linear framebuffer discovery, pixel format adaptation, tear-free double buffering,
 * damage region tracking, and bounds-validated surface access.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "limine.h"

typedef struct rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

typedef struct framebuffer {
    uint32_t *front_buffer; /* Hardware MMIO VRAM */
    uint32_t *back_buffer;  /* System RAM double buffer */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;        /* Bytes per scanline */
    uint32_t  pixels_per_pitch; /* Pitch / 4 */
    uint32_t  bpp;          /* Bits per pixel (must be 32) */

    uint8_t   red_shift;
    uint8_t   green_shift;
    uint8_t   blue_shift;

    rect_t    clip;         /* Active drawing clipping boundary */

    /* Damage tracking for efficient dirty-region flushing */
    bool      is_damaged;
    int32_t   damage_x0;
    int32_t   damage_y0;
    int32_t   damage_x1;
    int32_t   damage_y1;

    bool      initialized;
} framebuffer_t;

/* Initialize primary graphics framebuffer subsystem from bootloader response */
bool fb_init(const struct limine_framebuffer *lfb);

/* Retrieve primary desktop framebuffer */
framebuffer_t *fb_get_primary(void);

/* Check if graphics framebuffer is available */
bool fb_is_available(void);

/* Encode standard 0xRRGGBB into hardware pixel format */
static inline uint32_t fb_pack_color(framebuffer_t *fb, uint32_t rgb)
{
    if (!fb) return rgb;
    return (((rgb >> 16) & 0xFFu) << fb->red_shift) |
           (((rgb >> 8) & 0xFFu) << fb->green_shift) |
           ((rgb & 0xFFu) << fb->blue_shift);
}

/* Set active clipping rectangle */
void fb_set_clip(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h);

/* Reset clipping rectangle to full screen */
void fb_reset_clip(framebuffer_t *fb);

/* Mark region as damaged / dirty */
void fb_mark_damage(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h);

/* Flush damaged regions from back buffer to front hardware buffer */
void fb_flip(framebuffer_t *fb);

/* Force full-screen redraw and flush */
void fb_flip_full(framebuffer_t *fb);

/* Clear back buffer to color */
void fb_clear(framebuffer_t *fb, uint32_t rgb);

/* Print structured graphics diagnostic report */
void fb_dump_info(void);
