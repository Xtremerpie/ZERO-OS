/* ZERO KERNEL - ACPI Discovery & Parsing Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "hardware/acpi.h"
#include "memory/vmm.h"
#include "zero/log.h"
#include "zero/string.h"

static bool g_acpi_available = false;
static uint8_t g_acpi_revision = 0;
static const acpi_sdt_header_t *g_acpi_tables[ACPI_MAX_TABLES];
static size_t g_acpi_table_count = 0;

static struct acpi_madt_info g_madt_info;
static struct acpi_hpet_info g_hpet_info;

bool acpi_validate_checksum(const void *table, size_t length)
{
    if (!table || length == 0)
        return false;

    const uint8_t *bytes = (const uint8_t *)table;
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + bytes[i]);
    }
    return (sum == 0);
}

static const acpi_rsdp_t *acpi_scan_memory(uint64_t phys_start, uint64_t phys_end)
{
    for (uint64_t addr = phys_start; addr <= phys_end - sizeof(acpi_rsdp_t); addr += 16) {
        const char *sig = (const char *)vmm_phys_to_virt(addr);
        if (memcmp(sig, "RSD PTR ", 8) == 0) {
            const acpi_rsdp_t *candidate = (const acpi_rsdp_t *)sig;
            if (acpi_validate_checksum(candidate, sizeof(acpi_rsdp_t))) {
                return candidate;
            }
        }
    }
    return NULL;
}

static void acpi_parse_madt(const acpi_sdt_header_t *table)
{
    if (!table || table->length < sizeof(acpi_sdt_header_t) + 8)
        return;

    memset(&g_madt_info, 0, sizeof(g_madt_info));

    const uint8_t *data = (const uint8_t *)table;
    uint32_t lapic_addr = *(const uint32_t *)(data + sizeof(acpi_sdt_header_t));
    uint32_t flags = *(const uint32_t *)(data + sizeof(acpi_sdt_header_t) + 4);

    g_madt_info.lapic_phys_addr = (uint64_t)lapic_addr;
    g_madt_info.flags = flags;

    const uint8_t *curr = data + sizeof(acpi_sdt_header_t) + 8;
    const uint8_t *end = data + table->length;

    while (curr + sizeof(madt_record_header_t) <= end) {
        const madt_record_header_t *rec = (const madt_record_header_t *)curr;
        if (rec->length == 0 || curr + rec->length > end)
            break;

        switch (rec->type) {
        case MADT_TYPE_LOCAL_APIC: {
            if (rec->length >= sizeof(madt_local_apic_t) && g_madt_info.cpu_count < ACPI_MAX_CPUS) {
                const madt_local_apic_t *lapic = (const madt_local_apic_t *)rec;
                if (lapic->flags & 1) { /* Enabled */
                    g_madt_info.cpu_apic_ids[g_madt_info.cpu_count++] = lapic->apic_id;
                }
            }
            break;
        }
        case MADT_TYPE_IO_APIC: {
            if (rec->length >= sizeof(madt_io_apic_t) && g_madt_info.ioapic_count < ACPI_MAX_IOAPIC) {
                const madt_io_apic_t *ioapic = (const madt_io_apic_t *)rec;
                size_t idx = g_madt_info.ioapic_count++;
                g_madt_info.ioapics[idx].id = ioapic->ioapic_id;
                g_madt_info.ioapics[idx].phys_addr = ioapic->ioapic_address;
                g_madt_info.ioapics[idx].gsi_base = ioapic->gsi_base;
            }
            break;
        }
        case MADT_TYPE_INTERRUPT_OVERRIDE: {
            if (rec->length >= sizeof(madt_iso_t) && g_madt_info.iso_count < ACPI_MAX_ISO) {
                const madt_iso_t *iso = (const madt_iso_t *)rec;
                size_t idx = g_madt_info.iso_count++;
                g_madt_info.isos[idx].bus = iso->bus;
                g_madt_info.isos[idx].source_irq = iso->source_irq;
                g_madt_info.isos[idx].gsi = iso->gsi;
                g_madt_info.isos[idx].flags = iso->flags;
            }
            break;
        }
        case MADT_TYPE_LOCAL_APIC_OVERRIDE: {
            if (rec->length >= 12) {
                g_madt_info.lapic_phys_addr = *(const uint64_t *)(curr + 4);
            }
            break;
        }
        default:
            break;
        }

        curr += rec->length;
    }

    KLOG_INFO("acpi", "Parsed MADT: LAPIC=0x%016lx, CPUs=%u, IO-APICs=%u, ISOs=%u",
              g_madt_info.lapic_phys_addr, g_madt_info.cpu_count,
              g_madt_info.ioapic_count, g_madt_info.iso_count);
}

static void acpi_parse_hpet(const acpi_sdt_header_t *table)
{
    if (!table || table->length < sizeof(acpi_sdt_header_t) + 16)
        return;

    const uint8_t *data = (const uint8_t *)table + sizeof(acpi_sdt_header_t);
    g_hpet_info.event_timer_block_id = *(const uint32_t *)data;

    /* Generic Address Structure at offset 4: 12 bytes. Address is at offset 4 within GAS (total +8) */
    g_hpet_info.base_address = *(const uint64_t *)(data + 8);
    g_hpet_info.hpet_number = *(const uint8_t *)(data + 16);
    g_hpet_info.min_clock_tick = *(const uint16_t *)(data + 17);
    g_hpet_info.available = true;

    KLOG_INFO("acpi", "Parsed HPET #%u at MMIO 0x%016lx (id: 0x%08x)",
              g_hpet_info.hpet_number, g_hpet_info.base_address, g_hpet_info.event_timer_block_id);
}

static inline size_t rsd2_len(const acpi_rsdp2_t *r)
{
    return (r->length >= sizeof(acpi_rsdp2_t)) ? r->length : sizeof(acpi_rsdp2_t);
}

bool acpi_init(void *rsdp_address)
{
    const acpi_rsdp_t *rsdp = (const acpi_rsdp_t *)rsdp_address;

    /* If no address was provided by bootloader, perform memory search in standard BIOS regions */
    if (!rsdp) {
        rsdp = acpi_scan_memory(0x9FC00, 0x9FFFF); /* EBDA */
    }
    if (!rsdp) {
        rsdp = acpi_scan_memory(0xE0000, 0xFFFFF); /* BIOS ROM */
    }

    if (!rsdp) {
        KLOG_WARN("acpi", "ACPI RSDP not found; continuing without ACPI tables");
        return false;
    }

    if (!acpi_validate_checksum(rsdp, sizeof(acpi_rsdp_t))) {
        KLOG_ERR("acpi", "ACPI RSDP checksum failed");
        return false;
    }

    g_acpi_revision = rsdp->revision;
    g_acpi_table_count = 0;
    g_hpet_info.available = false;

    KLOG_INFO("acpi", "ACPI RSDP valid (Revision %u, OEM: '%.6s')", g_acpi_revision, rsdp->oem_id);

    /* Use XSDT (64-bit pointers) if ACPI 2.0+ and non-zero, otherwise RSDT (32-bit pointers) */
    if (g_acpi_revision >= 2) {
        const acpi_rsdp2_t *rsdp2 = (const acpi_rsdp2_t *)rsdp;
        if (rsdp2->length >= sizeof(acpi_rsdp2_t) && acpi_validate_checksum(rsdp2, rsd2_len(rsdp2))) {
            uint64_t xsdt_phys = rsdp2->xsdt_address;
            if (xsdt_phys) {
                const acpi_sdt_header_t *xsdt = (const acpi_sdt_header_t *)vmm_phys_to_virt(xsdt_phys);
                if (memcmp(xsdt->signature, "XSDT", 4) == 0 && acpi_validate_checksum(xsdt, xsdt->length)) {
                    size_t entry_count = (xsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
                    const uint64_t *ptrs = (const uint64_t *)((const uint8_t *)xsdt + sizeof(acpi_sdt_header_t));

                    for (size_t i = 0; i < entry_count && g_acpi_table_count < ACPI_MAX_TABLES; i++) {
                        const acpi_sdt_header_t *tbl = (const acpi_sdt_header_t *)vmm_phys_to_virt(ptrs[i]);
                        if (acpi_validate_checksum(tbl, tbl->length)) {
                            g_acpi_tables[g_acpi_table_count++] = tbl;
                        }
                    }
                }
            }
        }
    }

    /* Fallback to RSDT if XSDT was absent or produced 0 tables */
    if (g_acpi_table_count == 0 && rsdp->rsdt_address != 0) {
        const acpi_sdt_header_t *rsdt = (const acpi_sdt_header_t *)vmm_phys_to_virt((uint64_t)rsdp->rsdt_address);
        if (memcmp(rsdt->signature, "RSDT", 4) == 0 && acpi_validate_checksum(rsdt, rsdt->length)) {
            size_t entry_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
            const uint32_t *ptrs = (const uint32_t *)((const uint8_t *)rsdt + sizeof(acpi_sdt_header_t));

            for (size_t i = 0; i < entry_count && g_acpi_table_count < ACPI_MAX_TABLES; i++) {
                const acpi_sdt_header_t *tbl = (const acpi_sdt_header_t *)vmm_phys_to_virt((uint64_t)ptrs[i]);
                if (acpi_validate_checksum(tbl, tbl->length)) {
                    g_acpi_tables[g_acpi_table_count++] = tbl;
                }
            }
        }
    }

    KLOG_INFO("acpi", "Discovered %lu ACPI tables", g_acpi_table_count);

    /* Parse key tables */
    const acpi_sdt_header_t *madt = acpi_find_table("APIC");
    if (madt) {
        acpi_parse_madt(madt);
    }

    const acpi_sdt_header_t *hpet = acpi_find_table("HPET");
    if (hpet) {
        acpi_parse_hpet(hpet);
    }

    g_acpi_available = true;
    return true;
}

bool acpi_is_available(void)
{
    return g_acpi_available;
}

const acpi_sdt_header_t *acpi_find_table(const char *signature)
{
    if (!signature)
        return NULL;

    for (size_t i = 0; i < g_acpi_table_count; i++) {
        if (memcmp(g_acpi_tables[i]->signature, signature, 4) == 0) {
            return g_acpi_tables[i];
        }
    }
    return NULL;
}

const struct acpi_madt_info *acpi_get_madt_info(void)
{
    return g_acpi_available ? &g_madt_info : NULL;
}

const struct acpi_hpet_info *acpi_get_hpet_info(void)
{
    return (g_acpi_available && g_hpet_info.available) ? &g_hpet_info : NULL;
}

void acpi_dump_info(void)
{
    if (!g_acpi_available) {
        KLOG_INFO("acpi", "ACPI not available");
        return;
    }

    KLOG_INFO("acpi", "===== ZERO OS ACPI SYSTEM REPORT =====");
    KLOG_INFO("acpi", "  Revision: ACPI %s", (g_acpi_revision >= 2) ? "2.0+" : "1.0");
    KLOG_INFO("acpi", "  Total tables discovered: %lu", g_acpi_table_count);

    for (size_t i = 0; i < g_acpi_table_count; i++) {
        const acpi_sdt_header_t *t = g_acpi_tables[i];
        KLOG_INFO("acpi", "    Table #%02lu: [%.4s] Len: %5u Rev: %u OEM: '%.6s'",
                  i + 1, t->signature, t->length, t->revision, t->oem_id);
    }

    if (g_madt_info.lapic_phys_addr) {
        KLOG_INFO("acpi", "  MADT LAPIC: 0x%016lx (CPUs: %u, IOAPICs: %u)",
                  g_madt_info.lapic_phys_addr, g_madt_info.cpu_count, g_madt_info.ioapic_count);
    }

    if (g_hpet_info.available) {
        KLOG_INFO("acpi", "  HPET #%u: 0x%016lx (MinTick: %u)",
                  g_hpet_info.hpet_number, g_hpet_info.base_address, g_hpet_info.min_clock_tick);
    }
    KLOG_INFO("acpi", "=======================================");
}
