/* ZERO KERNEL - COM1 serial port (the primary diagnostic channel). */
#pragma once

#include <stdbool.h>

/* Initialise COM1 at 115200 8N1 and run a loopback self-check.
 * Returns false (and stays silent) if the UART is absent or fails loopback. */
bool serial_init(void);

/* Write one character; '\n' is sent as "\r\n". No-op when serial is unavailable. */
void serial_putc(char c);

bool serial_available(void);
