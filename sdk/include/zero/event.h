/* ZERO SDK - Application Event Definitions (Milestone 6.0.0 ZERO Ecosystem).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ZERO_EVENT_NONE = 0,
    ZERO_EVENT_PAINT,
    ZERO_EVENT_MOUSE_DOWN,
    ZERO_EVENT_MOUSE_UP,
    ZERO_EVENT_MOUSE_MOVE,
    ZERO_EVENT_KEY_DOWN,
    ZERO_EVENT_KEY_UP,
    ZERO_EVENT_CLOSE
} zero_event_type_t;

typedef struct zero_event {
    zero_event_type_t type;
    int32_t mouse_x;
    int32_t mouse_y;
    uint8_t mouse_buttons;
    uint8_t key_char;
    uint8_t key_scancode;
    uint32_t modifiers;
} zero_event_t;
