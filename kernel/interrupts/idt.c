/* ZERO KERNEL - IDT and CPU exception dispatch (vectors 0-31 only).
 *
 * No hardware interrupts (PIC/APIC), timers or IRQs exist in 0.0.0; interrupts
 * stay disabled. Only #BP is recoverable; every other exception panics. */

#include <stddef.h>
#include <stdint.h>
#include "zero/idt.h"
#include "zero/gdt.h"
#include "zero/log.h"
#include "zero/panic.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* The frame layout is an ABI with kernel/arch/x86_64/isr.asm: fail the build if it drifts. */
_Static_assert(offsetof(struct interrupt_frame, r15) == 0 * 8, "frame: r15 first (pushed last)");
_Static_assert(offsetof(struct interrupt_frame, rax) == 14 * 8, "frame: rax (pushed first)");
_Static_assert(offsetof(struct interrupt_frame, vector) == 15 * 8, "frame: vector");
_Static_assert(offsetof(struct interrupt_frame, error_code) == 16 * 8, "frame: error code");
_Static_assert(offsetof(struct interrupt_frame, rip) == 17 * 8, "frame: CPU-pushed rip");
_Static_assert(offsetof(struct interrupt_frame, ss) == 21 * 8, "frame: CPU-pushed ss");
_Static_assert(sizeof(struct interrupt_frame) == 22 * 8, "frame size");
_Static_assert(sizeof(struct idt_entry) == 16, "IDT gate is 16 bytes");

#define IDT_ENTRIES 256
#define GATE_INTERRUPT 0x8E /* present, DPL0, 64-bit interrupt gate */

extern void *const isr_stub_table[48]; /* kernel/arch/x86_64/isr.asm */

static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(16)));
static volatile uint64_t breakpoint_count;
static irq_handler_t irq_handlers[IDT_ENTRIES];

static const char *const names[32] = {
    "#DE Divide Error", "#DB Debug", "NMI", "#BP Breakpoint",
    "#OF Overflow", "#BR Bound Range Exceeded", "#UD Invalid Opcode",
    "#NM Device Not Available", "#DF Double Fault", "Coprocessor Segment Overrun",
    "#TS Invalid TSS", "#NP Segment Not Present", "#SS Stack-Segment Fault",
    "#GP General Protection", "#PF Page Fault", "Reserved (15)",
    "#MF x87 Floating-Point", "#AC Alignment Check", "#MC Machine Check",
    "#XM SIMD Floating-Point", "#VE Virtualization", "#CP Control Protection",
    "Reserved (22)", "Reserved (23)", "Reserved (24)", "Reserved (25)",
    "Reserved (26)", "Reserved (27)", "#HV Hypervisor Injection",
    "#VC VMM Communication", "#SX Security", "Reserved (31)",
};

const char *exception_name(uint64_t vector)
{
    return vector < 32 ? names[vector] : "Hardware Interrupt";
}

uint64_t exception_breakpoint_count(void)
{
    return breakpoint_count;
}

static void set_gate(unsigned vector, void *handler, uint8_t ist)
{
    uint64_t a = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low = a & 0xFFFF;
    idt[vector].selector = GDT_KERNEL_CS;
    idt[vector].ist = ist;
    idt[vector].type_attr = GATE_INTERRUPT;
    idt[vector].offset_mid = (a >> 16) & 0xFFFF;
    idt[vector].offset_high = (uint32_t)(a >> 32);
    idt[vector].zero = 0;
}

void idt_register_handler(unsigned vector, irq_handler_t handler)
{
    if (vector < IDT_ENTRIES) {
        irq_handlers[vector] = handler;
        if (vector < 48) {
            set_gate(vector, isr_stub_table[vector], 0);
        }
    }
}

void idt_init(void)
{
    for (unsigned v = 0; v < 48; v++)
        set_gate(v, isr_stub_table[v], v == 8 ? IST_DOUBLE_FAULT : 0);

    struct idtr r = { .limit = sizeof idt - 1, .base = (uint64_t)(uintptr_t)idt };
    __asm__ volatile("lidt %0" : : "m"(r) : "memory");

    KLOG_INFO("idt", "loaded: 48 interrupt/exception gates (0-31 exceptions, 32-47 IRQs), #DF on IST%d",
              IST_DOUBLE_FAULT);
}

void exception_dispatch(struct interrupt_frame *frame)
{
    if (frame->vector < 32) {
        if (frame->vector == 3) {
            breakpoint_count++;
            KLOG_INFO("exception", "#BP breakpoint at rip=0x%016lx (handled, resuming)", frame->rip);
            return;
        }
        kpanic_exception(frame);
    }

    /* Hardware interrupt vector >= 32 */
    if (frame->vector < IDT_ENTRIES && irq_handlers[frame->vector]) {
        irq_handlers[frame->vector](frame);
        return;
    }

    KLOG_WARN("interrupt", "unhandled interrupt vector 0x%02lx at rip=0x%016lx",
              frame->vector, frame->rip);
}
}
