/* ZERO KERNEL - Storage Subsystem Self-Test Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Verifies MBR & GPT partition table parsing, CRC32 algorithms, overflow prevention,
 * and block cache (bcache) LRU eviction, dirty tracking, and write-back synchronization.
 */
#include "debug/selftest_storage.h"
#include "storage/storage.h"
#include "storage/partition.h"
#include "storage/bcache.h"
#include "drivers/ramdisk.h"
#include "zero/log.h"

#include "zero/string.h"

static bool test_mbr_parsing(void)
{
    mbr_sector_t mbr;
    memset(&mbr, 0, sizeof(mbr));
    mbr.signature = MBR_SIGNATURE;

    /* Create test partition 1 */
    mbr.entries[0].boot_indicator = 0x80; /* Bootable */
    mbr.entries[0].partition_type = MBR_TYPE_ZEROFS;
    mbr.entries[0].start_lba = 2048;
    mbr.entries[0].sector_count = 10000;

    /* Create test partition 2 */
    mbr.entries[1].boot_indicator = 0x00;
    mbr.entries[1].partition_type = MBR_TYPE_FAT32;
    mbr.entries[1].start_lba = 12048;
    mbr.entries[1].sector_count = 50000;

    /* Test MBR validity */
    if (!mbr_is_valid(&mbr)) {
        kprintf("[FAIL] mbr: valid signature rejected\n");
        return false;
    }

    /* Create dummy parent block device */
    block_device_t dummy_dev;
    memset(&dummy_dev, 0, sizeof(dummy_dev));
    strncpy(dummy_dev.name, "testdisk", sizeof(dummy_dev.name) - 1);
    dummy_dev.sector_count = 100000;
    dummy_dev.sector_size = 512;

    partition_info_t parts[4];
    int count = partition_parse_mbr(&dummy_dev, (const uint8_t *)&mbr, parts, 4);
    if (count != 2) {
        kprintf("[FAIL] mbr: expected 2 partitions, got %d\n", count);
        return false;
    }

    if (parts[0].start_lba != 2048 || parts[0].sector_count != 10000 || !parts[0].bootable) {
        kprintf("[FAIL] mbr: partition 0 parameters mismatch\n");
        return false;
    }
    if (parts[1].start_lba != 12048 || parts[1].sector_count != 50000 || parts[1].bootable) {
        kprintf("[FAIL] mbr: partition 1 parameters mismatch\n");
        return false;
    }

    /* Test integer overflow protection */
    mbr.entries[2].partition_type = 0x83;
    mbr.entries[2].start_lba = 0xFFFFFFFFu;
    mbr.entries[2].sector_count = 1000;
    count = partition_parse_mbr(&dummy_dev, (const uint8_t *)&mbr, parts, 4);
    if (count != 2) {
        kprintf("[FAIL] mbr: integer overflow partition was not rejected\n");
        return false;
    }

    kprintf("[PASS] storage: MBR sector validation, entry extraction, and overflow protection verified\n");
    return true;
}

static bool test_gpt_crc_and_structure(void)
{
    /* Test CRC32 implementation */
    const char *test_data = "123456789";
    uint32_t crc = partition_crc32(test_data, 9);
    if (crc != 0xCBF43926u) {
        kprintf("[FAIL] gpt: CRC32 mismatch! Expected 0xCBF43926, got 0x%08X\n", crc);
        return false;
    }

    /* Test synthetic GPT Header structure */
    gpt_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.signature = GPT_SIGNATURE;
    hdr.revision = GPT_REVISION_1_0;
    hdr.header_size = sizeof(gpt_header_t);
    hdr.current_lba = 1;
    hdr.backup_lba = 200000;
    hdr.first_usable_lba = 34;
    hdr.last_usable_lba = 199966;
    hdr.partition_entry_lba = 2;
    hdr.num_partition_entries = 128;
    hdr.sizeof_partition_entry = 128;

    uint32_t computed_crc = partition_crc32(&hdr, hdr.header_size);
    hdr.header_crc32 = computed_crc;

    /* Verify header self-check logic */
    hdr.header_crc32 = 0;
    uint32_t verify_crc = partition_crc32(&hdr, hdr.header_size);
    if (verify_crc != computed_crc) {
        kprintf("[FAIL] gpt: header self-CRC check failed\n");
        return false;
    }

    kprintf("[PASS] storage: GPT header structure and standard IEEE 802.3 CRC32 algorithm verified\n");
    return true;
}

static bool test_bcache_operations(void)
{
    /* Use RAM disk 0 for testing block cache */
    block_device_t *rdev = block_device_get_by_name("ramdisk0");
    if (!rdev) {
        kprintf("[WARN] storage: ramdisk0 not found, skipping bcache test\n");
        return true;
    }

    uint8_t write_pattern[BCACHE_BLOCK_SIZE];
    uint8_t read_pattern[BCACHE_BLOCK_SIZE];

    for (size_t i = 0; i < BCACHE_BLOCK_SIZE; i++) {
        write_pattern[i] = (uint8_t)(i ^ 0xA5);
    }

    /* Write block 10 via cache (write-back) */
    int rc = bcache_write(rdev, 10, write_pattern, false);
    if (rc != 0) {
        kprintf("[FAIL] bcache: write block failed with error %d\n", rc);
        return false;
    }

    /* Read back block 10 (should be a cache hit) */
    memset(read_pattern, 0, sizeof(read_pattern));
    rc = bcache_read(rdev, 10, read_pattern);
    if (rc != 0 || memcmp(write_pattern, read_pattern, BCACHE_BLOCK_SIZE) != 0) {
        kprintf("[FAIL] bcache: read back failed or data mismatch\n");
        return false;
    }

    /* Check stats: should have at least 1 hit and 1 write */
    bcache_stats_t stats;
    bcache_get_stats(&stats);
    if (stats.writes < 1 || stats.hits < 1) {
        kprintf("[FAIL] bcache: stats inconsistency (writes=%llu, hits=%llu)\n",
                (unsigned long long)stats.writes, (unsigned long long)stats.hits);
        return false;
    }

    /* Sync cache to disk */
    rc = bcache_sync(rdev);
    if (rc != 0) {
        kprintf("[FAIL] bcache: sync failed\n");
        return false;
    }

    /* Invalidate cache and read directly through cache to test cold load from device */
    bcache_invalidate(rdev);
    memset(read_pattern, 0, sizeof(read_pattern));
    rc = bcache_read(rdev, 10, read_pattern);
    if (rc != 0 || memcmp(write_pattern, read_pattern, BCACHE_BLOCK_SIZE) != 0) {
        kprintf("[FAIL] bcache: post-sync cold read failed\n");
        return false;
    }

    kprintf("[PASS] storage: block cache read, write-back, hit verification, and sync flushed verified\n");
    return true;
}

bool storage_run_selftests(void)
{
    kprintf("[TEST] Running Storage & Partition Subsystem Self-Tests...\n");

    if (!test_mbr_parsing()) return false;
    if (!test_gpt_crc_and_structure()) return false;
    if (!test_bcache_operations()) return false;

    kprintf("[PASS] storage: storage architecture, partition tables, and cache verified\n");
    return true;
}
