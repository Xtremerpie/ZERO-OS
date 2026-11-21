/* ZERO KERNEL - Native Desktop Notification System (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Lightweight toast notification overlay rendered above the taskbar in the bottom-right corner.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"

#define NOTIFICATION_MAX_TITLE   32u
#define NOTIFICATION_MAX_MSG     64u
#define NOTIFICATION_WIDTH       240
#define NOTIFICATION_HEIGHT      52

typedef struct notification {
    char     title[NOTIFICATION_MAX_TITLE];
    char     msg[NOTIFICATION_MAX_MSG];
    uint8_t  icon_glyph;
    uint32_t duration_ms;
    uint32_t elapsed_ms;
    bool     active;
} notification_t;

/* Initialize desktop notification subsystem */
void notification_init(void);

/* Display toast notification */
void notification_show(const char *title, const char *msg, uint8_t icon_glyph, uint32_t duration_ms);

/* Dismiss currently visible notification */
void notification_dismiss(void);

/* Update notification lifecycle timers */
void notification_tick(uint32_t delta_ms);

/* Check if notification is currently active and visible */
bool notification_is_visible(void);

/* Render notification toast onto the framebuffer */
void notification_render(framebuffer_t *fb);
