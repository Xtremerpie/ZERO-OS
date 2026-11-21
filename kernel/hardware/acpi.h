/* ZERO KERNEL - ACPI Discovery & Parsing Interface (Milestone 3.0.0 Hardware Foundation).
 *
 * Provides RSDP discovery, RSDT/XSDT validation, table lookups,
 * and parsing of MADT (Multiple APIC Description Table) and HPET.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ACPI_MAX_TABLES 32u
#define ACPI_MAX_CPUS   32u
#define ACPI_MAX_IOAPIC 4u
#define ACPI_MAX_ISO    16u

/* ACPI SDT Standard Header */
typedef struct __attribute__((packed)) {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

/* ACPI 1.0 RSDP */
typedef struct __attribute__((packed)) {
    char signature[8]; /* "RSD PTR " */
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} acpi_rsdp_t;

/* ACPI 2.0+ Extended RSDP */
typedef struct __attribute__((packed)) {
    acpi_rsdp_t rsdp1;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} acpi_rsdp2_t;

/* MADT Record Types */
#define MADT_TYPE_LOCAL_APIC           0
#define MADT_TYPE_IO_APIC              1
#define MADT_TYPE_INTERRUPT_OVERRIDE   2
#define MADT_TYPE_NMI_SOURCE           3
#define MADT_TYPE_LOCAL_APIC_NMI       4
#define MADT_TYPE_LOCAL_APIC_OVERRIDE  5

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} madt_record_header_t;

typedef struct __attribute__((packed)) {
    madt_record_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags; /* Bit 0: Enabled, Bit 1: Online capable */
} madt_local_apic_t;

typedef struct __attribute__((packed)) {
    madt_record_header_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
} madt_io_apic_t;

typedef struct __attribute__((packed)) {
    madt_record_header_t header;
    uint8_t bus;
    uint8_t source_irq;
    uint32_t gsi;
    uint16_t flags;
} madt_iso_t;

/* Parsed MADT summary info */
struct acpi_madt_info {
    uint64_t lapic_phys_addr;
    uint32_t flags;

    uint32_t cpu_count;
    uint8_t cpu_apic_ids[ACPI_MAX_CPUS];

    uint32_t ioapic_count;
    struct {
        uint8_t id;
        uint32_t phys_addr;
        uint32_t gsi_base;
    } ioapics[ACPI_MAX_IOAPIC];

    uint32_t iso_count;
    struct {
        uint8_t bus;
        uint8_t source_irq;
        uint32_t gsi;
        uint16_t flags;
    } isos[ACPI_MAX_ISO];
};

/* HPET table info */
struct acpi_hpet_info {
    uint32_t event_timer_block_id;
    uint64_t base_address;
    uint8_t hpet_number;
    uint16_t min_clock_tick;
    bool available;
};

/* Initialize ACPI subsystem using RSDP address pointer (or scans memory if NULL) */
bool acpi_init(void *rsdp_address);

/* Check if ACPI tables were successfully discovered */
bool acpi_is_available(void);

/* Find a table by its 4-character signature (e.g. "APIC", "HPET", "FACP") */
const acpi_sdt_header_t *acpi_find_table(const char *signature);

/* Validate table checksum */
bool acpi_validate_checksum(const void *table, size_t length);

/* Retrieve parsed MADT information */
const struct acpi_madt_info *acpi_get_madt_info(void);

/* Retrieve parsed HPET information */
const struct acpi_hpet_info *acpi_get_hpet_info(void);

/* Print discovered ACPI configuration to diagnostic log */
void acpi_dump_info(void);
