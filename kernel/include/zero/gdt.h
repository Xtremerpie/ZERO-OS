/* ZERO KERNEL - global descriptor table and task state segment. */
#pragma once

#define GDT_KERNEL_CS 0x08
#define GDT_KERNEL_DS 0x10
#define GDT_TSS_SEL   0x18
#define GDT_USER_DS   0x28
#define GDT_USER_CS   0x30

#define IST_DOUBLE_FAULT 1

/* Loads a flat 64-bit GDT, user segment descriptors, and a TSS. */
void gdt_init(void);

/* Update the kernel stack pointer (rsp0) in the TSS for the active thread. */
void gdt_set_kernel_stack(uint64_t stack_top);
