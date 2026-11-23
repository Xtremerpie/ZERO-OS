/* ZERO KERNEL - PS/2 Mouse Driver Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/ps2_mouse.h"
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

static uint8_t g_mouse_cycle = 0;
static uint8_t g_mouse_bytes[3];
static uint32_t g_prev_buttons = 0;
static device_t g_mouse_dev;

static inline void ps2_wait_write(void)
{
    for (int timeout = 0; timeout < 10000; timeout++) {
        if ((inb(PS2_STATUS_PORT) & 2) == 0)
            return;
        cpu_pause();
    }
}

static inline void ps2_wait_read(void)
{
    for (int timeout = 0; timeout < 10000; timeout++) {
        if (inb(PS2_STATUS_PORT) & 1)
            return;
        cpu_pause();
    }
}

void ps2_mouse_handle_byte(uint8_t byte)
{
    switch (g_mouse_cycle) {
    case 0:
        /* Bit 3 of byte 0 must be 1 in standard PS/2 mouse protocol */
        if ((byte & 0x08) == 0) {
            return; /* Out of sync */
        }
        g_mouse_bytes[0] = byte;
        g_mouse_cycle = 1;
        break;

    case 1:
        g_mouse_bytes[1] = byte;
        g_mouse_cycle = 2;
        break;

    case 2:
        g_mouse_bytes[2] = byte;
        g_mouse_cycle = 0;

        /* Check for overflow flags in byte 0 */
        if (g_mouse_bytes[0] & 0xC0) {
            return; /* Discard overflowed movement */
        }

        /* Calculate signed X delta */
        int32_t dx = (int32_t)g_mouse_bytes[1];
        if (g_mouse_bytes[0] & 0x10) { /* X sign bit */
            dx |= (int32_t)0xFFFFFF00;
        }

        /* Calculate signed Y delta (PS/2 Y axis is inverted relative to screen) */
        int32_t dy = (int32_t)g_mouse_bytes[2];
        if (g_mouse_bytes[0] & 0x20) { /* Y sign bit */
            dy |= (int32_t)0xFFFFFF00;
        }

        /* Extract buttons */
        uint32_t cur_buttons = 0;
        if (g_mouse_bytes[0] & 0x01) cur_buttons |= MOUSE_BTN_LEFT;
        if (g_mouse_bytes[0] & 0x02) cur_buttons |= MOUSE_BTN_RIGHT;
        if (g_mouse_bytes[0] & 0x04) cur_buttons |= MOUSE_BTN_MIDDLE;

        /* Push movement event if delta is non-zero */
        if (dx != 0 || dy != 0) {
            input_event_t ev;
            ev.type = INPUT_MOUSE_MOVE;
            ev.code = cur_buttons;
            ev.dx = dx;
            ev.dy = -dy; /* Standard screen coordinates */
            ev.modifiers = input_get_modifiers();
            ev.timestamp = 0;
            input_push_event(&ev);
        }

        /* Check button transitions */
        uint32_t changed = cur_buttons ^ g_prev_buttons;
        if (changed) {
            for (uint32_t b = 1; b <= 4; b <<= 1) {
                if (changed & b) {
                    input_event_t ev;
                    ev.type = (cur_buttons & b) ? INPUT_MOUSE_BUTTON_DOWN : INPUT_MOUSE_BUTTON_UP;
                    ev.code = b;
                    ev.dx = 0;
                    ev.dy = 0;
                    ev.modifiers = input_get_modifiers();
                    ev.timestamp = 0;
                    input_push_event(&ev);
                }
            }
            g_prev_buttons = cur_buttons;
        }
        break;

    default:
        g_mouse_cycle = 0;
        break;
    }
}

static void ps2_mouse_irq_handler(struct interrupt_frame *frame)
{
    (void)frame;
    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & 1) && (status & 0x20)) { /* Output buffer full + mouse bit set */
        uint8_t b = inb(PS2_DATA_PORT);
        ps2_mouse_handle_byte(b);
    }
    apic_eoi();
}

int ps2_mouse_init(void)
{
    g_mouse_cycle = 0;
    g_prev_buttons = 0;

    /* 1. Enable 2nd PS/2 port (auxiliary mouse device) */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xA8);

    /* 2. Read Controller Configuration Byte */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0x20);
    ps2_wait_read();
    uint8_t compaq_status = inb(PS2_DATA_PORT);

    /* 3. Enable IRQ 12 (bit 1) and disable mouse clock disable (clear bit 5) */
    compaq_status |= 0x02;
    compaq_status &= (uint8_t)~0x20;

    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0x60);
    ps2_wait_write();
    outb(PS2_DATA_PORT, compaq_status);

    /* 4. Send default settings to mouse */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4);
    ps2_wait_write();
    outb(PS2_DATA_PORT, 0xF6); /* Set defaults */
    ps2_wait_read();
    (void)inb(PS2_DATA_PORT);  /* ACK (0xFA) */

    /* 5. Enable data reporting */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4);
    ps2_wait_write();
    outb(PS2_DATA_PORT, 0xF4); /* Enable data reporting */
    ps2_wait_read();
    (void)inb(PS2_DATA_PORT);  /* ACK (0xFA) */

    /* Register IRQ 12 (vector 0x2C) in IDT */
    idt_register_handler(0x2C, ps2_mouse_irq_handler);

    /* Register device with Driver Manager */
    memset(&g_mouse_dev, 0, sizeof(device_t));
    strcpy(g_mouse_dev.name, "PS/2 Mouse");
    g_mouse_dev.type = DEVICE_TYPE_INPUT;
    g_mouse_dev.state = DEVICE_STATE_ACTIVE;
    g_mouse_dev.resource_count = 2;
    g_mouse_dev.resources[0].type = RES_IO_PORT;
    g_mouse_dev.resources[0].base = PS2_DATA_PORT;
    g_mouse_dev.resources[0].size = 1;
    g_mouse_dev.resources[1].type = RES_IRQ;
    g_mouse_dev.resources[1].irq = 12;

    device_register(&g_mouse_dev);

    KLOG_INFO("ps2", "PS/2 mouse driver initialized (IRQ 12, vector 0x2C)");
    return 0;
}
