/* ZERO KERNEL - PS/2 Keyboard Driver Interface (Milestone 3.0.0 Hardware Foundation). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Initialize PS/2 Keyboard controller and interrupt handler */
int ps2_kbd_init(void);

/* Process a raw scancode directly (useful for tests and hardware IRQ) */
void ps2_kbd_handle_scancode(uint8_t scancode);
