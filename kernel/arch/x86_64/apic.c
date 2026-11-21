/* ZERO KERNEL - Local APIC Implementation (Version 1.0.0 Core Foundation). */

#include "zero/apic.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "memory/vmm.h"

static bool apic_initialized = false;
static volatile uint32_t *apic_base_virt = NULL;

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
