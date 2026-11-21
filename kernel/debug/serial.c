/* ZERO KERNEL - COM1 16550 UART driver (115200 8N1, polled, no interrupts). */

#include "zero/serial.h"
#include "zero/cpu.h"

#define COM1 0x3F8

#define REG_DATA   0 /* DLAB=0: data;    DLAB=1: divisor low  */
#define REG_IER    1 /* DLAB=0: int en;  DLAB=1: divisor high */
#define REG_FCR    2
#define REG_LCR    3
#define REG_MCR    4
#define REG_LSR    5

#define LSR_THR_EMPTY 0x20

static bool serial_ok;

bool serial_init(void)
{
    outb(COM1 + REG_IER, 0x00); /* no interrupts */
    outb(COM1 + REG_LCR, 0x80); /* DLAB on */
    outb(COM1 + REG_DATA, 0x01); /* divisor 1 = 115200 baud */
    outb(COM1 + REG_IER, 0x00);
    outb(COM1 + REG_LCR, 0x03); /* 8 bits, no parity, 1 stop, DLAB off */
    outb(COM1 + REG_FCR, 0xC7); /* enable + clear FIFOs, 14-byte threshold */
    outb(COM1 + REG_MCR, 0x1E); /* loopback mode, OUT1/OUT2 set */

    outb(COM1 + REG_DATA, 0xAE); /* loopback self-check */
    if (inb(COM1 + REG_DATA) != 0xAE) {
        serial_ok = false;
        return false;
    }

    outb(COM1 + REG_MCR, 0x0F); /* normal operation, DTR/RTS/OUT1/OUT2 */
    serial_ok = true;
    return true;
}

bool serial_available(void)
{
    return serial_ok;
}

static void serial_raw(char c)
{
    /* Bounded wait so a dead UART can never hang the kernel. */
    for (unsigned i = 0; i < 100000; i++) {
        if (inb(COM1 + REG_LSR) & LSR_THR_EMPTY)
            break;
    }
    outb(COM1 + REG_DATA, (uint8_t)c);
}

void serial_putc(char c)
{
    if (!serial_ok)
        return;
    if (c == '\n')
        serial_raw('\r');
    serial_raw(c);
}
