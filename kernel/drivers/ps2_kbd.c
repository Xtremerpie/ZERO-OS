/* ZERO KERNEL - PS/2 Keyboard Driver Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/ps2_kbd.h"
#include "drivers/input.h"
#include "drivers/driver.h"
#include "zero/cpu.h"
#include "zero/idt.h"
#include "zero/apic.h"
#include "zero/log.h"
#include "zero/string.h"

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

static bool g_extended = false;
static device_t g_kbd_dev;

/* Scancode Set 1 make code mapping table (0x00 to 0x58) */
static const uint32_t scancode_set1_map[0x59] = {
    [0x00] = KEY_UNKNOWN,
    [0x01] = KEY_ESCAPE,
    [0x02] = KEY_1, [0x03] = KEY_2, [0x04] = KEY_3, [0x05] = KEY_4,
    [0x06] = KEY_5, [0x07] = KEY_6, [0x08] = KEY_7, [0x09] = KEY_8,
    [0x0A] = KEY_9, [0x0B] = KEY_0,
    [0x0C] = KEY_MINUS, [0x0D] = KEY_EQUALS, [0x0E] = KEY_BACKSPACE,
    [0x0F] = KEY_TAB,
    [0x10] = KEY_Q, [0x11] = KEY_W, [0x12] = KEY_E, [0x13] = KEY_R,
    [0x14] = KEY_T, [0x15] = KEY_Y, [0x16] = KEY_U, [0x17] = KEY_I,
    [0x18] = KEY_O, [0x19] = KEY_P,
    [0x1A] = KEY_LEFTBRACKET, [0x1B] = KEY_RIGHTBRACKET,
    [0x1C] = KEY_ENTER,
    [0x1D] = KEY_LCTRL,
    [0x1E] = KEY_A, [0x1F] = KEY_S, [0x20] = KEY_D, [0x21] = KEY_F,
    [0x22] = KEY_G, [0x23] = KEY_H, [0x24] = KEY_J, [0x25] = KEY_K,
    [0x26] = KEY_L,
    [0x27] = KEY_SEMICOLON, [0x28] = KEY_APOSTROPHE, [0x29] = KEY_GRAVE,
    [0x2A] = KEY_LSHIFT, [0x2B] = KEY_BACKSLASH,
    [0x2C] = KEY_Z, [0x2D] = KEY_X, [0x2E] = KEY_C, [0x2F] = KEY_V,
    [0x30] = KEY_B, [0x31] = KEY_N, [0x32] = KEY_M,
    [0x33] = KEY_COMMA, [0x34] = KEY_PERIOD, [0x35] = KEY_SLASH,
    [0x36] = KEY_RSHIFT,
    [0x38] = KEY_LALT,
    [0x39] = KEY_SPACE,
    [0x3A] = KEY_CAPSLOCK,
    [0x3B] = KEY_F1, [0x3C] = KEY_F2, [0x3D] = KEY_F3, [0x3E] = KEY_F4,
    [0x3F] = KEY_F5, [0x40] = KEY_F6, [0x41] = KEY_F7, [0x42] = KEY_F8,
    [0x43] = KEY_F9, [0x44] = KEY_F10,
    [0x57] = KEY_F11, [0x58] = KEY_F12
};

void ps2_kbd_handle_scancode(uint8_t scancode)
{
    if (scancode == 0xE0) {
        g_extended = true;
        return;
    }

    bool is_break = (scancode & 0x80) != 0;
    uint8_t make = scancode & 0x7F;

    uint32_t keycode = KEY_UNKNOWN;
    if (g_extended) {
        switch (make) {
        case 0x1D: keycode = KEY_RCTRL; break;
        case 0x38: keycode = KEY_RALT; break;
        case 0x48: keycode = KEY_UP; break;
        case 0x4B: keycode = KEY_LEFT; break;
        case 0x4D: keycode = KEY_RIGHT; break;
        case 0x50: keycode = KEY_DOWN; break;
        case 0x47: keycode = KEY_HOME; break;
        case 0x4F: keycode = KEY_END; break;
        case 0x49: keycode = KEY_PAGEUP; break;
        case 0x51: keycode = KEY_PAGEDOWN; break;
        case 0x52: keycode = KEY_INSERT; break;
        case 0x53: keycode = KEY_DELETE; break;
        case 0x5B: keycode = KEY_LWIN; break;
        case 0x5C: keycode = KEY_RWIN; break;
        default:   keycode = KEY_UNKNOWN; break;
        }
        g_extended = false;
    } else {
        if (make < sizeof(scancode_set1_map) / sizeof(scancode_set1_map[0])) {
            keycode = scancode_set1_map[make];
        }
    }

    if (keycode == KEY_UNKNOWN)
        return;

    /* Update modifier states */
    if (keycode == KEY_LSHIFT || keycode == KEY_RSHIFT) {
        input_set_modifier(MODIFIER_SHIFT, !is_break);
    } else if (keycode == KEY_LCTRL || keycode == KEY_RCTRL) {
        input_set_modifier(MODIFIER_CTRL, !is_break);
    } else if (keycode == KEY_LALT || keycode == KEY_RALT) {
        input_set_modifier(MODIFIER_ALT, !is_break);
    } else if (keycode == KEY_CAPSLOCK && !is_break) {
        uint32_t cur = input_get_modifiers();
        input_set_modifier(MODIFIER_CAPSLOCK, !(cur & MODIFIER_CAPSLOCK));
    }

    input_event_t ev;
    ev.type = is_break ? INPUT_KEY_UP : INPUT_KEY_DOWN;
    ev.code = keycode;
    ev.dx = 0;
    ev.dy = 0;
    ev.modifiers = input_get_modifiers();
    ev.timestamp = 0;

    input_push_event(&ev);
}

static inline void ps2_kbd_wait_write(void)
{
    for (int timeout = 0; timeout < 10000; timeout++) {
        if ((inb(PS2_STATUS_PORT) & 2) == 0)
            return;
        cpu_pause();
    }
}

static inline bool ps2_kbd_wait_read(void)
{
    for (int timeout = 0; timeout < 10000; timeout++) {
        if (inb(PS2_STATUS_PORT) & 1)
            return true;
        cpu_pause();
    }
    return false;
}

void ps2_flush_output(void)
{
    for (int i = 0; i < 64; i++) {
        if ((inb(PS2_STATUS_PORT) & 1) == 0)
            break;
        (void)inb(PS2_DATA_PORT);
    }
}

static void ps2_kbd_irq_handler(struct interrupt_frame *frame)
{
    (void)frame;
    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & 1) && !(status & 0x20)) { /* Output buffer full, keyboard source */
        uint8_t sc = inb(PS2_DATA_PORT);
        ps2_kbd_handle_scancode(sc);
    }
    apic_eoi();
}

int ps2_kbd_init(void)
{
    g_extended = false;

    /* Enable first PS/2 port and flush any stale controller output. */
    ps2_kbd_wait_write();
    outb(PS2_COMMAND_PORT, 0xAE);
    ps2_flush_output();

    /* Enable IRQ 1 (bit 0) and port 1 clock (clear bit 4) in i8042 config. */
    ps2_kbd_wait_write();
    outb(PS2_COMMAND_PORT, 0x20);
    if (ps2_kbd_wait_read()) {
        uint8_t cfg = inb(PS2_DATA_PORT);
        cfg |= 0x01u;
        cfg &= (uint8_t)~0x10u;
        ps2_kbd_wait_write();
        outb(PS2_COMMAND_PORT, 0x60);
        ps2_kbd_wait_write();
        outb(PS2_DATA_PORT, cfg);
    }
    ps2_flush_output();

    /* Register IRQ 1 (vector 0x21) in IDT */
    idt_register_handler(0x21, ps2_kbd_irq_handler);

    /* Register device with Driver Manager */
    memset(&g_kbd_dev, 0, sizeof(device_t));
    strcpy(g_kbd_dev.name, "PS/2 Keyboard");
    g_kbd_dev.type = DEVICE_TYPE_INPUT;
    g_kbd_dev.state = DEVICE_STATE_ACTIVE;
    g_kbd_dev.resource_count = 2;
    g_kbd_dev.resources[0].type = RES_IO_PORT;
    g_kbd_dev.resources[0].base = PS2_DATA_PORT;
    g_kbd_dev.resources[0].size = 1;
    g_kbd_dev.resources[1].type = RES_IRQ;
    g_kbd_dev.resources[1].irq = 1;

    device_register(&g_kbd_dev);

    KLOG_INFO("ps2", "PS/2 keyboard driver initialized (IRQ 1, vector 0x21)");
    return 0;
}
