/* ZERO KERNEL - early framebuffer console (text and banner drawing).
 * Framebuffer only; this is not a graphics stack. Colours are 0xRRGGBB. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "limine.h"

/* Accepts only 32 bpp RGB framebuffers. Returns false otherwise (kernel continues serial-only). */
bool fbcon_init(const struct limine_framebuffer *fb);
bool fbcon_available(void);

uint32_t fbcon_width(void);
uint32_t fbcon_height(void);

void fbcon_clear(uint32_t rgb);
void fbcon_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);

/* Draw text with the 8x8 font scaled by an integer factor. Returns the x just past the text. */
uint32_t fbcon_draw_string(uint32_t x, uint32_t y, const char *s, uint32_t scale, uint32_t rgb);

/* Text console: lines are written below `top_px` and scroll within the area under it. */
void fbcon_console_begin(uint32_t top_px, uint32_t scale);
void fbcon_set_fg(uint32_t rgb);
void fbcon_putc(char c);
