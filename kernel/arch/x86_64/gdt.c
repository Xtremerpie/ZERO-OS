/* ZERO KERNEL - GDT and TSS.
 *
 * Flat 64-bit kernel code/data segments plus one TSS whose IST1 stack is used
 * only by the double-fault handler, so a #DF caused by a broken kernel stack
 * can still be reported. There are no user segments in 0.0.0. */

#include <stdint.h>
#include "zero/gdt.h"
#include "zero/log.h"
#include "zero/string.h"

struct tss {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

struct gdt_table {
    uint64_t null;
    uint64_t kernel_code;
    uint64_t kernel_data;
    uint64_t tss_low;
    uint64_t tss_high;
    uint64_t user_data;
    uint64_t user_code;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define DF_STACK_SIZE 16384

/* The structs are packed (alignment 1), so force the alignment the CPU prefers. */
_Static_assert(sizeof(struct tss) == 104, "TSS must be 104 bytes");
_Static_assert(sizeof(struct gdt_table) == 56, "GDT layout: null, code, data, 16-byte TSS, user_data, user_code");

static struct tss tss __attribute__((aligned(16)));
static struct gdt_table gdt __attribute__((aligned(16)));
static uint8_t df_stack[DF_STACK_SIZE] __attribute__((aligned(16)));

static void set_tss_descriptor(uint64_t base, uint32_t limit)
{
    gdt.tss_low = (limit & 0xFFFFull) |
                  ((base & 0xFFFFFFull) << 16) |
                  (0x89ull << 40) | /* present, type 0x9 = available 64-bit TSS */
                  (((uint64_t)limit & 0xF0000ull) << 32) |
                  (((base >> 24) & 0xFFull) << 56);
    gdt.tss_high = base >> 32;
}

void gdt_init(void)
{
    memset(&tss, 0, sizeof tss);
    tss.ist[IST_DOUBLE_FAULT - 1] = (uint64_t)(uintptr_t)(df_stack + DF_STACK_SIZE);
    tss.iopb_offset = sizeof tss; /* no I/O permission bitmap */

    gdt.null = 0;
    gdt.kernel_code = 0x00AF9A000000FFFFull; /* L=1, present, DPL0, exec/read */
    gdt.kernel_data = 0x00CF92000000FFFFull; /* present, DPL0, read/write */
    set_tss_descriptor((uint64_t)(uintptr_t)&tss, sizeof tss - 1);
    gdt.user_data   = 0x00CFF2000000FFFFull; /* present, DPL3, read/write */
    gdt.user_code   = 0x00AFFA000000FFFFull; /* L=1, present, DPL3, exec/read */

    struct gdtr r = { .limit = sizeof gdt - 1, .base = (uint64_t)(uintptr_t)&gdt };

    __asm__ volatile(
        "lgdt %0\n\t"
        "pushq %1\n\t"          /* new CS */
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"               /* far return reloads CS */
        "1:\n\t"
        "movw %2, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "xorl %%eax, %%eax\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %3, %%ax\n\t"
        "ltr %%ax\n\t"
        :
        : "m"(r), "i"((uint64_t)GDT_KERNEL_CS), "i"((uint16_t)GDT_KERNEL_DS),
          "i"((uint16_t)GDT_TSS_SEL)
        : "rax", "memory");

    KLOG_INFO("gdt", "loaded: CS=0x%02x DS=0x%02x TSS=0x%02x, #DF stack IST1 at %p",
              GDT_KERNEL_CS, GDT_KERNEL_DS, GDT_TSS_SEL, (void *)(df_stack + DF_STACK_SIZE));
}

void gdt_set_kernel_stack(uint64_t stack_top)
{
    tss.rsp[0] = stack_top;
}
