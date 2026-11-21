/* ZERO KERNEL - interrupt descriptor table and CPU exception handling. */
#pragma once

#include <stdint.h>

/* Register state pushed by kernel/arch/x86_64/isr.asm. Field order is an ABI with
 * that file: do not reorder without updating it. */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code; /* 0 for vectors that push none */
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct interrupt_frame *frame);

/* Register a handler callback for an interrupt vector (vectors 32-255). */
void idt_register_handler(unsigned vector, irq_handler_t handler);

/* Installs handlers for CPU exception vectors 0-31 and hardware IRQs 32-47. */
void idt_init(void);

/* Called from the assembly stubs. Returns only for recoverable exceptions (#BP). */
void exception_dispatch(struct interrupt_frame *frame);

/* Number of #BP exceptions handled and resumed so far (used by the selftest). */
uint64_t exception_breakpoint_count(void);

const char *exception_name(uint64_t vector);
