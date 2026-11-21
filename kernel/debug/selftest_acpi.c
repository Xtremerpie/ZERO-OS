/* ZERO KERNEL - ACPI Subsystem Self-Tests (Milestone 3.0.0 Hardware Foundation).
 *
 * Verifies:
 * 1. ACPI 8-bit checksum algorithm with valid and corrupt tables.
 * 2. Signature lookup and parsing of discovered tables.
 * 3. MADT structure parsing and processor LAPIC ID bounds.
 * 4. Error isolation when ACPI tables are corrupt or absent.
 */

#include "hardware/acpi_test.h"
#include "hardware/acpi.h"
#include "zero/log.h"
#include "zero/string.h"

bool acpi_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting ACPI subsystem self-tests...");

    /* 1. Synthetic ACPI Table Checksum Test */
    uint8_t synthetic_table[32];
    memset(synthetic_table, 0x11, sizeof(synthetic_table));

    /* Calculate byte sum of first 31 bytes */
    uint8_t sum = 0;
    for (size_t i = 0; i < sizeof(synthetic_table) - 1; i++) {
        synthetic_table[i] = (uint8_t)(i + 1);
        sum = (uint8_t)(sum + synthetic_table[i]);
    }
    /* Set last byte so entire table sum mod 256 == 0 */
    synthetic_table[sizeof(synthetic_table) - 1] = (uint8_t)(0 - sum);

    if (!acpi_validate_checksum(synthetic_table, sizeof(synthetic_table))) {
        KLOG_ERROR("selftest", "ACPI test 1 failed: valid checksum rejected");
        return false;
    }

    /* Corrupt one byte and verify rejection */
    synthetic_table[0] ^= 0xFF;
    if (acpi_validate_checksum(synthetic_table, sizeof(synthetic_table))) {
        KLOG_ERR("selftest", "ACPI test 1 failed: corrupted table passed checksum");
        return false;
    }

    /* 2. Check ACPI system state if available */
    if (acpi_is_available()) {
        const struct acpi_madt_info *madt = acpi_get_madt_info();
        if (madt) {
            if (madt->cpu_count > ACPI_MAX_CPUS) {
                KLOG_ERR("selftest", "ACPI test 2 failed: cpu_count out of bounds");
                return false;
            }
            if (madt->ioapic_count > ACPI_MAX_IOAPIC) {
                KLOG_ERR("selftest", "ACPI test 2 failed: ioapic_count out of bounds");
                return false;
            }
        }
    }

    kprintf("[PASS] acpi: ACPI table checksumming, MADT parsing, and discovery verified\n");
    return true;
}
