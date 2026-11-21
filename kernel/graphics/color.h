/* ZERO KERNEL - Graphical Color Palette & Visual Identity (Milestone 5.0.0 ZERO Desktop).
 *
 * Defines the official ZERO OS visual identity palette:
 * Deep Black, Dark Graphite, Metallic Gold, and Crimson Red.
 */
#pragma once

#include <stdint.h>

/* Primary Visual Identity Colors (0x00RRGGBB) */
#define COLOR_ZERO_BLACK        0x08080Au /* Deep Void Black */
#define COLOR_ZERO_DARK_BASE    0x121216u /* Primary Desktop Graphite Base */
#define COLOR_ZERO_PANEL        0x1A1A20u /* Window/Card Panel Background */
#define COLOR_ZERO_PANEL_ALT    0x22222Au /* Secondary/Elevated Surface */
#define COLOR_ZERO_BORDER       0x2E2E3Au /* Window & Control Outline */
#define COLOR_ZERO_BORDER_FOCUS 0xD4AF37u /* Focused Window Outline (Gold) */

#define COLOR_ZERO_GOLD         0xD4AF37u /* Signature Metallic Gold */
#define COLOR_ZERO_GOLD_HOVER   0xE4C158u /* Highlight / Hover Gold */
#define COLOR_ZERO_GOLD_MUTED   0x8A7324u /* Inactive / Dim Gold */

#define COLOR_ZERO_CRIMSON      0xC41230u /* Signature Crimson Red */
#define COLOR_ZERO_CRIMSON_GLOW 0xE6194Bu /* Alert / Close Button Hover */

#define COLOR_ZERO_WHITE        0xF2F2F4u /* High-Contrast Text */
#define COLOR_ZERO_MUTED        0x8A8A96u /* Subtitle / Placeholder Text */
#define COLOR_ZERO_DISABLED     0x4A4A54u /* Disabled Control State */

#define COLOR_ZERO_SUCCESS      0x00D26Au /* Operation Success */
#define COLOR_ZERO_INFO         0x2A85FFu /* Informational Badge */
#define COLOR_ZERO_WARNING      0xF5A623u /* System Warning */

#define COLOR_ZERO_TASKBAR      0x0E0E12u /* Bottom System Taskbar */
#define COLOR_ZERO_TITLEBAR_ACT 0x1E1E26u /* Active Window Titlebar */
#define COLOR_ZERO_TITLEBAR_INA 0x14141Au /* Inactive Window Titlebar */

/* Color packing and component extraction helpers */
static inline uint32_t color_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline uint8_t color_get_r(uint32_t rgb) { return (uint8_t)((rgb >> 16) & 0xFFu); }
static inline uint8_t color_get_g(uint32_t rgb) { return (uint8_t)((rgb >> 8) & 0xFFu); }
static inline uint8_t color_get_b(uint32_t rgb) { return (uint8_t)(rgb & 0xFFu); }
