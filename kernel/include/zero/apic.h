/* ZERO KERNEL - Local APIC Subsystem (Version 1.0.0 Core Foundation).
 *
 * Implements:
 * - APIC base MSR discovery and virtual MMIO mapping.
 * - Legacy 8259 PIC masking.
 * - Spurious Vector and Task Priority configuration.
 * - End-Of-Interrupt (EOI) signaling.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APIC_BASE_MSR          0x1B
#define APIC_BASE_MSR_ENABLE   (1ull << 11)
#define APIC_BASE_MSR_ADDR     0x000FFFFFFFFFF000ull

/* Standard Local APIC MMIO Register Offsets */
#define APIC_REG_ID            0x0020
#define APIC_REG_VERSION       0x0030
#define APIC_REG_TPR           0x0080 /* Task Priority Register */
#define APIC_REG_EOI           0x00B0 /* End of Interrupt */
#define APIC_REG_SVR           0x00F0 /* Spurious Interrupt Vector */
#define APIC_REG_ESR           0x0280 /* Error Status */
#define APIC_REG_ICR_LOW       0x0300 /* Interrupt Command (0-31) */
#define APIC_REG_ICR_HIGH      0x0310 /* Interrupt Command (32-63) */
#define APIC_REG_LVT_TIMER     0x0320
#define APIC_REG_LVT_LINT0     0x0350
#define APIC_REG_LVT_LINT1     0x0360
#define APIC_REG_LVT_ERROR     0x0370
#define APIC_REG_TIMER_INITCNT 0x0380
#define APIC_REG_TIMER_CURRCNT 0x0390
#define APIC_REG_TIMER_DIV     0x03E0

#define APIC_SPURIOUS_VECTOR   0xFF
#define APIC_TIMER_VECTOR      0x20 /* Vector 32 (first user interrupt vector) */

/* Initialize the Local APIC and mask legacy 8259 PIC. */
bool apic_init(void);

/* Check if Local APIC is initialized. */
bool apic_is_initialized(void);

/* Read a 32-bit APIC MMIO register. */
uint32_t apic_read(uint32_t reg_offset);

/* Write a 32-bit APIC MMIO register. */
void apic_write(uint32_t reg_offset, uint32_t val);

/* Signal End-Of-Interrupt to the Local APIC. */
void apic_eoi(void);

/* Retrieve APIC ID of current CPU. */
uint32_t apic_get_id(void);

/* Initialize I/O APIC from ACPI MADT (or default 0xFEC00000) and mask all GSIs. */
bool ioapic_init(void);

/* Route an ISA IRQ (respecting ACPI MADT Interrupt Source Overrides) to an IDT vector. */
bool ioapic_route_irq(uint8_t irq, uint8_t vector);

/* Compatibility name used by early PS/2 driver revisions. */
#define apic_send_eoi() apic_eoi()
