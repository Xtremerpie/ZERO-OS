/* ZERO KERNEL - Unified Input Subsystem Interface (Milestone 3.0.0 Hardware Foundation).
 *
 * Defines virtual keycodes, mouse button states, modifier flags,
 * and a thread-safe circular event queue for hardware input events.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define INPUT_QUEUE_SIZE 128u

/* Modifiers */
#define MODIFIER_SHIFT    (1u << 0)
#define MODIFIER_CTRL     (1u << 1)
#define MODIFIER_ALT      (1u << 2)
#define MODIFIER_CAPSLOCK (1u << 3)

/* Mouse Buttons */
#define MOUSE_BTN_LEFT    (1u << 0)
#define MOUSE_BTN_RIGHT   (1u << 1)
#define MOUSE_BTN_MIDDLE  (1u << 2)

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_KEY_DOWN,
    INPUT_KEY_UP,
    INPUT_MOUSE_MOVE,
    INPUT_MOUSE_BUTTON_DOWN,
    INPUT_MOUSE_BUTTON_UP
} input_event_type_t;

/* Virtual Key Codes */
enum {
    KEY_UNKNOWN = 0,
    KEY_A = 1, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_SPACE,
    KEY_MINUS,
    KEY_EQUALS,
    KEY_LEFTBRACKET,
    KEY_RIGHTBRACKET,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_GRAVE,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_CAPSLOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_PRINTSCREEN, KEY_SCROLLLOCK, KEY_PAUSE,
    KEY_INSERT, KEY_HOME, KEY_PAGEUP,
    KEY_DELETE, KEY_END, KEY_PAGEDOWN,
    KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP,
    KEY_LCTRL, KEY_LSHIFT, KEY_LALT,
    KEY_RCTRL, KEY_RSHIFT, KEY_RALT
};

typedef struct {
    input_event_type_t type;
    uint32_t code;      /* Virtual keycode or mouse button bit */
    int32_t dx;         /* Relative mouse horizontal delta */
    int32_t dy;         /* Relative mouse vertical delta */
    uint32_t modifiers; /* Active modifier flags */
    uint64_t timestamp; /* System ticks at event time */
} input_event_t;

/* Initialize input event subsystem */
void input_init(void);

/* Push an event into the circular input queue */
int input_push_event(const input_event_t *ev);

/* Poll next event from queue (non-blocking). Returns true if event retrieved */
bool input_poll_event(input_event_t *out_ev);

/* Get current active modifier flags */
uint32_t input_get_modifiers(void);

/* Update modifier state */
void input_set_modifier(uint32_t mod, bool active);

/* Convert a virtual keycode to ASCII character based on modifier state */
char input_keycode_to_ascii(uint32_t keycode, uint32_t modifiers);
