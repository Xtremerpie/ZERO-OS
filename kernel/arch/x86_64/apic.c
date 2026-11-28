/* ZERO KERNEL - Local APIC Implementation (Version 1.0.0 Core Foundation). */

#include "zero/apic.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "hardware/acpi.h"
#include "memory/vmm.h"

#define IOAPIC_DEFAULT_PHYS   0xFEC00000ull
#define IOAPIC_REG_ID         0x00u
#define IOAPIC_REG_VER        0x01u
#define IOAPIC_REG_REDTBL(n)  (0x10u + 2u * (uint32_t)(n))

#define IOAPIC_PIN_POLARITY_LOW  (1u << 13)
#define IOAPIC_TRIGGER_LEVEL     (1u << 15)
#define IOAPIC_MASKED            (1u << 16)

static bool apic_initialized = false;
static volatile uint32_t *apic_base_virt = NULL;

static bool ioapic_initialized = false;
static volatile uint32_t *ioapic_base_virt = NULL;
static uint32_t ioapic_gsi_base = 0;
static uint32_t ioapic_max_redir = 0;

static void pic_disable_legacy(void)
{
    /* Mask all 8259 PIC interrupts on master and slave */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    KLOG_INFO("apic", "legacy 8259 PIC masked (ports 0x21, 0xA1 disabled)");
}

bool apic_init(void)
{
    /* Check APIC presence via CPUID */
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1u << 9))) {
        KLOG_ERROR("apic", "CPUID reports Local APIC not supported");
        return false;
    }

    /* Mask legacy PIC first */
    pic_disable_legacy();

    /* Read APIC Base MSR */
    uint64_t apic_msr = rdmsr(APIC_BASE_MSR);
    if (!(apic_msr & APIC_BASE_MSR_ENABLE)) {
        apic_msr |= APIC_BASE_MSR_ENABLE;
        wrmsr(APIC_BASE_MSR, apic_msr);
    }

    uint64_t apic_base_phys = apic_msr & APIC_BASE_MSR_ADDR;
    apic_base_virt = (volatile uint32_t *)vmm_phys_to_virt(apic_base_phys);

    KLOG_INFO("apic", "Local APIC physical base: 0x%016lx (virtual: %p)",
              apic_base_phys, (void *)apic_base_virt);

    /* Software enable APIC by setting bit 8 in Spurious Vector Register */
    apic_write(APIC_REG_SVR, (1u << 8) | APIC_SPURIOUS_VECTOR);

    /* Set Task Priority Register to 0 (accept all interrupts) */
    apic_write(APIC_REG_TPR, 0);

    apic_initialized = true;
    KLOG_INFO("apic", "Local APIC software enabled, SVR=0x%08x, ID=0x%08x",
              apic_read(APIC_REG_SVR), apic_get_id());

    return true;
}

bool apic_is_initialized(void)
{
    return apic_initialized;
}

uint32_t apic_read(uint32_t reg_offset)
{
    if (!apic_base_virt)
        return 0;
    return apic_base_virt[reg_offset / 4];
}

void apic_write(uint32_t reg_offset, uint32_t val)
{
    if (!apic_base_virt)
        return;
    apic_base_virt[reg_offset / 4] = val;
}

void apic_eoi(void)
{
    apic_write(APIC_REG_EOI, 0);
}

uint32_t apic_get_id(void)
{
    return apic_read(APIC_REG_ID) >> 24;
}

static uint32_t ioapic_read(uint32_t reg)
{
    if (!ioapic_base_virt)
        return 0;
    ioapic_base_virt[0] = reg;
    return ioapic_base_virt[4];
}

static void ioapic_write(uint32_t reg, uint32_t val)
{
    if (!ioapic_base_virt)
        return;
    ioapic_base_virt[0] = reg;
    ioapic_base_virt[4] = val;
}

bool ioapic_init(void)
{
    if (!apic_initialized)
        return false;

    uint64_t phys = IOAPIC_DEFAULT_PHYS;
    ioapic_gsi_base = 0;

    const struct acpi_madt_info *madt = acpi_get_madt_info();
    if (madt && madt->ioapic_count > 0) {
        phys = (uint64_t)madt->ioapics[0].phys_addr;
        ioapic_gsi_base = madt->ioapics[0].gsi_base;
    }

    if (phys != IOAPIC_DEFAULT_PHYS) {
        uint64_t *pml4 = vmm_get_kernel_pml4_virt();
        if (!pml4 || vmm_map_page(pml4, (uint64_t)(uintptr_t)vmm_phys_to_virt(phys), phys,
                                  VMM_FLAG_WRITABLE | VMM_FLAG_NX | VMM_FLAG_NOCACHE) != VMM_OK) {
            KLOG_ERROR("ioapic", "failed to map I/O APIC MMIO at 0x%016lx", phys);
            return false;
        }
    }

    ioapic_base_virt = (volatile uint32_t *)vmm_phys_to_virt(phys);
    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    ioapic_max_redir = ((ver >> 16) & 0xFFu) + 1u;
    if (ioapic_max_redir == 0 || ioapic_max_redir > 240u) {
        KLOG_ERROR("ioapic", "unexpected I/O APIC version register 0x%08x", ver);
        return false;
    }

    /* Mask all redirection entries until drivers explicitly register routes. */
    for (uint32_t i = 0; i < ioapic_max_redir; i++) {
        ioapic_write(IOAPIC_REG_REDTBL(i), IOAPIC_MASKED);
        ioapic_write(IOAPIC_REG_REDTBL(i) + 1u, 0);
    }

    ioapic_initialized = true;
    KLOG_INFO("ioapic", "I/O APIC initialized at 0x%016lx (GSI base %u, %u inputs)",
              phys, ioapic_gsi_base, ioapic_max_redir);
    return true;
}

bool ioapic_route_irq(uint8_t irq, uint8_t vector)
{
    if (!ioapic_initialized)
        return false;

    uint32_t gsi = irq;
    uint32_t flags = 0; /* ISA default: active-high, edge-triggered */

    const struct acpi_madt_info *madt = acpi_get_madt_info();
    if (madt) {
        for (uint32_t i = 0; i < madt->iso_count; i++) {
            if (madt->isos[i].bus == 0 && madt->isos[i].source_irq == irq) {
                gsi = madt->isos[i].gsi;
                uint16_t iso_flags = madt->isos[i].flags;
                if ((iso_flags & 0x03u) == 0x03u)
                    flags |= IOAPIC_PIN_POLARITY_LOW;
                if (((iso_flags >> 2) & 0x03u) == 0x03u)
                    flags |= IOAPIC_TRIGGER_LEVEL;
                break;
            }
        }
    }

    if (gsi < ioapic_gsi_base)
        return false;
    uint32_t pin = gsi - ioapic_gsi_base;
    if (pin >= ioapic_max_redir)
        return false;

    uint32_t lapic_id = apic_get_id();
    ioapic_write(IOAPIC_REG_REDTBL(pin) + 1u, lapic_id << 24);
    ioapic_write(IOAPIC_REG_REDTBL(pin), (uint32_t)vector | flags);

    KLOG_INFO("ioapic", "routed ISA IRQ %u -> GSI %u -> vector 0x%02x (LAPIC %u)",
              irq, gsi, vector, lapic_id);
    return true;
}
