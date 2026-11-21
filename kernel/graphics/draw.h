/* ZERO KERNEL - 2D Drawing Primitives & Vector Engine (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements high-performance, clipping-enforced 2D rendering primitives:
 * pixels, Bresenham lines, hollow/filled rectangles, rounded corners, circles,
 * linear gradients, and alpha blending.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"
#include "graphics/color.h"

/* Set single pixel with clipping */
void draw_pixel(framebuffer_t *fb, int32_t x, int32_t y, uint32_t color);

/* Blend pixel with alpha (0 = transparent, 255 = fully opaque) */
void draw_blend_pixel(framebuffer_t *fb, int32_t x, int32_t y, uint32_t color, uint8_t alpha);

/* Draw straight line using Bresenham's algorithm */
void draw_line(framebuffer_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

/* Draw hollow rectangle outline */
void draw_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/* Draw solid filled rectangle */
void draw_fill_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/* Draw rounded rectangle outline */
void draw_rounded_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color);

/* Draw solid filled rounded rectangle */
void draw_fill_rounded_rect(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t color);

/* Draw hollow circle outline */
void draw_circle(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t radius, uint32_t color);

/* Draw solid filled circle */
void draw_fill_circle(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t radius, uint32_t color);

/* Draw vertical linear gradient rectangle */
void draw_gradient_v(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color_top, uint32_t color_bottom);

/* Blit bitmap image to surface with optional alpha blending */
void draw_image(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t *pixels, bool has_alpha);
