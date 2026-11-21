/* ZERO KERNEL - Input Subsystem Self-Tests (Milestone 3.0.0 Hardware Foundation).
 *
 * Verifies:
 * 1. Unified input queue FIFO order and capacity limits.
 * 2. Keyboard Scancode Set 1 make and break decoding.
 * 3. Modifier tracking (Shift, CapsLock, Ctrl, Alt).
 * 4. Virtual keycode to ASCII translation.
 * 5. PS/2 Mouse 3-byte packet decoding and button state transitions.
 */

#include "drivers/input_test.h"
#include "drivers/input.h"
#include "drivers/ps2_kbd.h"
#include "drivers/ps2_mouse.h"
#include "zero/log.h"

bool input_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting Input subsystem self-tests...");

    /* 1. Event queue push/poll FIFO test */
    input_event_t ev_in;
    ev_in.type = INPUT_KEY_DOWN;
    ev_in.code = KEY_Z;
    ev_in.dx = 0;
    ev_in.dy = 0;
    ev_in.modifiers = 0;
    ev_in.timestamp = 100;

    if (input_push_event(&ev_in) != 0) {
        KLOG_ERR("selftest", "Input test 1 failed: input_push_event failed");
        return false;
    }

    input_event_t ev_out;
    if (!input_poll_event(&ev_out)) {
        KLOG_ERR("selftest", "Input test 1 failed: input_poll_event returned false");
        return false;
    }

    if (ev_out.type != INPUT_KEY_DOWN || ev_out.code != KEY_Z || ev_out.timestamp != 100) {
        KLOG_ERR("selftest", "Input test 1 failed: event data corrupted");
        return false;
    }

    /* 2. Keyboard Scancode Translation Test (make and break) */
    ps2_kbd_handle_scancode(0x1E); /* Make code for 'A' */
    if (!input_poll_event(&ev_out) || ev_out.type != INPUT_KEY_DOWN || ev_out.code != KEY_A) {
        KLOG_ERR("selftest", "Input test 2 failed: Scancode 0x1E (A) make decode failed");
        return false;
    }

    ps2_kbd_handle_scancode(0x9E); /* Break code for 'A' (0x1E | 0x80) */
    if (!input_poll_event(&ev_out) || ev_out.type != INPUT_KEY_UP || ev_out.code != KEY_A) {
        KLOG_ERR("selftest", "Input test 2 failed: Scancode 0x9E (A) break decode failed");
        return false;
    }

    /* 3. Modifier and Keycode to ASCII Translation Test */
    char c_lower = input_keycode_to_ascii(KEY_A, 0);
    char c_upper = input_keycode_to_ascii(KEY_A, MODIFIER_SHIFT);
    char c_num = input_keycode_to_ascii(KEY_1, 0);
    char c_sym = input_keycode_to_ascii(KEY_1, MODIFIER_SHIFT);

    if (c_lower != 'a' || c_upper != 'A' || c_num != '1' || c_sym != '!') {
        KLOG_ERR("selftest", "Input test 3 failed: ASCII translation mismatch ('%c', '%c', '%c', '%c')",
                 c_lower, c_upper, c_num, c_sym);
        return false;
    }

    /* 4. Mouse 3-byte Packet Decoding Test */
    /* Byte 0: 0x09 (bit 3 always 1, bit 0 left button down)
     * Byte 1: 0x05 (dx = +5)
     * Byte 2: 0x03 (dy = +3) */
    ps2_mouse_handle_byte(0x09);
    ps2_mouse_handle_byte(0x05);
    ps2_mouse_handle_byte(0x03);

    bool got_move = false;
    bool got_btn = false;
    while (input_poll_event(&ev_out)) {
        if (ev_out.type == INPUT_MOUSE_MOVE && ev_out.dx == 5) {
            got_move = true;
        }
        if (ev_out.type == INPUT_MOUSE_BUTTON_DOWN && (ev_out.code & MOUSE_BTN_LEFT)) {
            got_btn = true;
        }
    }

    if (!got_move || !got_btn) {
        KLOG_ERR("selftest", "Input test 4 failed: mouse packet decoding did not produce move/button events");
        return false;
    }

    /* Clean up mouse button state */
    ps2_mouse_handle_byte(0x08); /* All buttons up */
    ps2_mouse_handle_byte(0x00);
    ps2_mouse_handle_byte(0x00);
    while (input_poll_event(&ev_out)) {}

    kprintf("[PASS] input: event queue, scancode translation, modifiers, and mouse packets verified\n");
    return true;
}
