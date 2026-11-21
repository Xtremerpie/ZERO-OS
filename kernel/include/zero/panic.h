/* ZERO KERNEL - panic handler. A panic prints diagnostics and halts the machine. */
#pragma once

#include "zero/idt.h"

__attribute__((noreturn, format(printf, 1, 2)))
void kpanic(const char *fmt, ...);

/* Panic for an unrecoverable CPU exception: prints vector, error code and all registers. */
__attribute__((noreturn))
void kpanic_exception(const struct interrupt_frame *frame);
