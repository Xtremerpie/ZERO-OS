/* ZERO KERNEL - Typography & Native Font Rendering Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements glyph lookup, variable integer scaling, text measurement,
 * alignment helpers, UI icon glyphs, and UTF-8 validation.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"

#define FONT_BASE_WIDTH   8u
#define FONT_BASE_HEIGHT  8u

/* Special System UI Icon Glyphs (0x80..0x8A) */
#define GLYPH_ICON_MINIMIZE   0x80
#define GLYPH_ICON_MAXIMIZE   0x81
#define GLYPH_ICON_CLOSE      0x82
#define GLYPH_ICON_FOLDER     0x83
#define GLYPH_ICON_FILE       0x84
#define GLYPH_ICON_TERMINAL   0x85
#define GLYPH_ICON_GEAR       0x86
#define GLYPH_ICON_CPU        0x87
#define GLYPH_ICON_DISK       0x88
#define GLYPH_ICON_NET        0x89
#define GLYPH_ICON_CHECK      0x8A\n#define GLYPH_ICON_INFO       0x8B

/* Measure pixel width of string rendered at given scale */
int32_t font_get_string_width(const char *text, uint32_t scale);

/* Measure pixel height of text rendered at given scale */
int32_t font_get_string_height(uint32_t scale);

/* Render text string at (x, y) with active framebuffer clipping */
int32_t font_draw_string(framebuffer_t *fb, int32_t x, int32_t y, const char *text, uint32_t scale, uint32_t color);

/* Render text string at (x, y) with explicit custom clipping bounds */
int32_t font_draw_string_clipped(framebuffer_t *fb, int32_t x, int32_t y, const char *text, uint32_t scale, uint32_t color, const rect_t *clip);

/* Render text centered within a target bounding box */
void font_draw_string_centered(framebuffer_t *fb, int32_t box_x, int32_t box_y, int32_t box_w, int32_t box_h,
                               const char *text, uint32_t scale, uint32_t color);

/* Draw a single character glyph at (x, y) */
void font_draw_char(framebuffer_t *fb, int32_t x, int32_t y, uint8_t ch, uint32_t scale, uint32_t color);
