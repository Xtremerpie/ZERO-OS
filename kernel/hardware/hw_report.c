/* ZERO KERNEL - Hardware Diagnostics & Reporting Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "hardware/hw_report.h"
#include "hardware/acpi.h"
#include "hardware/pci.h"
#include "drivers/driver.h"
#include "drivers/block.h"
#include "drivers/usb.h"
#include "memory/pmm.h"
#include "zero/cpu.h"
#include "zero/apic.h"
#include "zero/fbcon.h"
#include "zero/log.h"
#include "zero/string.h"

void hw_report_generate(void)
{
    /* 1. CPU Identification */
    char vendor[13] = { 0 };
    uint32_t a, b, c, d;
    cpuid(0, 0, &a, (uint32_t *)&vendor[0], (uint32_t *)&vendor[8], (uint32_t *)&vendor[4]);
    vendor[12] = '\0';

    char brand[49] = { 0 };
    uint32_t max_ext = 0;
    cpuid(0x80000000, 0, &max_ext, &b, &c, &d);
    if (max_ext >= 0x80000004) {
        uint32_t *p = (uint32_t *)brand;
        cpuid(0x80000002, 0, &p[0], &p[1], &p[2], &p[3]);
        cpuid(0x80000003, 0, &p[4], &p[5], &p[6], &p[7]);
        cpuid(0x80000004, 0, &p[8], &p[9], &p[10], &p[11]);
        brand[48] = '\0';
    }

    cpuid(1, 0, &a, &b, &c, &d);
    bool has_sse = (d & (1u << 25)) != 0;
    bool has_sse2 = (d & (1u << 26)) != 0;
    bool has_apic = (d & (1u << 9)) != 0;
    bool has_x2apic = (c & (1u << 21)) != 0;

    /* 2. Memory Information */
    struct pmm_stats mem_stats;
    pmm_get_stats(&mem_stats);

    /* 3. Output Formatted Report */
    kprintf("\n");
    kprintf("============================================================\n");
    kprintf("  ZERO OS HARDWARE REPORT\n");
    kprintf("============================================================\n");

    kprintf("CPU:\n");
    kprintf("  Vendor:       %s\n", vendor);
    if (brand[0] != '\0') {
        kprintf("  Model:        %s\n", brand);
    }
    kprintf("  Architecture: x86_64 Long Mode\n");
    kprintf("  Features:     APIC:%s x2APIC:%s SSE:%s SSE2:%s\n",
            has_apic ? "YES" : "NO", has_x2apic ? "YES" : "NO",
            has_sse ? "YES" : "NO", has_sse2 ? "YES" : "NO");

    kprintf("MEMORY:\n");
    kprintf("  Total detected: %lu MiB (%lu frames)\n",
            mem_stats.total_memory / (1024 * 1024), mem_stats.total_frames);
    kprintf("  Usable RAM:     %lu MiB (%lu frames free)\n",
            mem_stats.usable_memory / (1024 * 1024), mem_stats.free_frames);

    kprintf("FIRMWARE & INTERRUPTS:\n");
    kprintf("  ACPI:           %s\n", acpi_is_available() ? "Available" : "Not Detected");
    const struct acpi_madt_info *madt = acpi_get_madt_info();
    if (madt) {
        kprintf("  MADT:           Available (CPUs: %u, IO-APICs: %u, LAPIC: 0x%08lx)\n",
                madt->cpu_count, madt->ioapic_count, madt->lapic_phys_addr);
    } else {
        kprintf("  MADT:           Not Detected\n");
    }
    kprintf("  Local APIC:     %s (Periodic Timer: 100 Hz)\n",
            apic_is_initialized() ? "Active (MMIO enabled)" : "Inactive");
    const struct acpi_hpet_info *hpet = acpi_get_hpet_info();
    kprintf("  HPET:           %s\n", hpet ? "Available" : "Not Detected");

    kprintf("PCI BUS:\n");
    size_t pci_count = pci_device_count();
    kprintf("  Devices discovered: %lu\n", pci_count);
    for (size_t i = 0; i < pci_count; i++) {
        pci_device_t *d = pci_get_device(i);
        if (d) {
            kprintf("    [%02x:%02x.%x] %04x:%04x - %s\n",
                    d->bus, d->slot, d->func, d->vendor_id, d->device_id,
                    pci_class_name(d->class_code, d->subclass));
        }
    }

    kprintf("INPUT:\n");
    device_t *kbd = device_get_by_name("PS/2 Keyboard");
    device_t *mouse = device_get_by_name("PS/2 Mouse");
    kprintf("  Keyboard:       %s\n", kbd ? "PS/2 Keyboard (IRQ 1 active)" : "Not Detected");
    kprintf("  Mouse:          %s\n", mouse ? "PS/2 Mouse (IRQ 12 active)" : "Not Detected");

    kprintf("STORAGE:\n");
    size_t blk_count = block_device_count();
    kprintf("  Devices discovered: %lu\n", blk_count);
    for (size_t i = 0; i < blk_count; i++) {
        block_device_t *b = block_device_get(i);
        if (b) {
            uint64_t mib = (b->sector_count * b->sector_size) / (1024 * 1024);
            kprintf("    '%s': %lu MiB (%lu sectors) [%s]\n",
                    b->name, mib, b->sector_count, b->read_only ? "RO" : "RW");
        }
    }

    kprintf("DISPLAY:\n");
    if (fbcon_available()) {
        kprintf("  Framebuffer:    %ux%u, 32 bpp linear RGB\n", fbcon_width(), fbcon_height());
    } else {
        kprintf("  Framebuffer:    Serial COM1 text fallback\n");
    }

    kprintf("============================================================\n");
    kprintf("\n");
}
