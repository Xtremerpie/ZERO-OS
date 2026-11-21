/* ZERO KERNEL - PS/2 Mouse Driver Interface (Milestone 3.0.0 Hardware Foundation). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Initialize PS/2 Mouse auxiliary device and interrupt handler */
int ps2_mouse_init(void);

/* Process a raw byte from the mouse data stream (useful for tests and hardware IRQ) */
void ps2_mouse_handle_byte(uint8_t byte);
