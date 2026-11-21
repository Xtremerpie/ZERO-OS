/* ZERO KERNEL - Block Device Subsystem Self-Tests (Milestone 3.0.0 Hardware Foundation).
 *
 * Verifies:
 * 1. Block device registration and parameter verification.
 * 2. Sector-granular write and read-back parity.
 * 3. Multi-sector sequential operations.
 * 4. Boundary enforcement (rejecting out-of-range LBA or overflow).
 * 5. Read-only device protection enforcement.
 */

#include "drivers/block_test.h"
#include "drivers/block.h"
#include "drivers/ramdisk.h"
#include "zero/log.h"
#include "zero/string.h"

bool block_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting Block Device subsystem self-tests...");

    block_device_t *bdev = ramdisk_get_device();
    if (!bdev) {
        KLOG_ERR("selftest", "Block test 1 failed: ramdisk0 device not found");
        return false;
    }

    if (bdev->sector_size != BLOCK_SECTOR_SIZE || bdev->sector_count == 0) {
        KLOG_ERR("selftest", "Block test 1 failed: invalid sector size or count");
        return false;
    }

    /* 2. Single-sector write and read-back verification */
    uint8_t write_buf[BLOCK_SECTOR_SIZE];
    uint8_t read_buf[BLOCK_SECTOR_SIZE];

    for (size_t i = 0; i < BLOCK_SECTOR_SIZE; i++) {
        write_buf[i] = (uint8_t)(i ^ 0x5A);
    }
    memset(read_buf, 0, BLOCK_SECTOR_SIZE);

    int res = block_write(bdev, 5, 1, write_buf);
    if (res != 0) {
        KLOG_ERR("selftest", "Block test 2 failed: block_write to LBA 5 failed (%d)", res);
        return false;
    }

    res = block_read(bdev, 5, 1, read_buf);
    if (res != 0 || memcmp(write_buf, read_buf, BLOCK_SECTOR_SIZE) != 0) {
        KLOG_ERR("selftest", "Block test 2 failed: read-back data mismatch at LBA 5");
        return false;
    }

    /* 3. Multi-sector write (4 sectors = 2048 bytes) */
    uint8_t multi_write[BLOCK_SECTOR_SIZE * 4];
    uint8_t multi_read[BLOCK_SECTOR_SIZE * 4];

    for (size_t i = 0; i < sizeof(multi_write); i++) {
        multi_write[i] = (uint8_t)(i & 0xFF);
    }
    memset(multi_read, 0, sizeof(multi_read));

    res = block_write(bdev, 10, 4, multi_write);
    if (res != 0) {
        KLOG_ERR("selftest", "Block test 3 failed: multi-sector write failed (%d)", res);
        return false;
    }

    res = block_read(bdev, 10, 4, multi_read);
    if (res != 0 || memcmp(multi_write, multi_read, sizeof(multi_write)) != 0) {
        KLOG_ERR("selftest", "Block test 3 failed: multi-sector read-back mismatch");
        return false;
    }

    /* 4. Out-of-bounds rejection */
    res = block_read(bdev, bdev->sector_count, 1, read_buf);
    if (res == 0) {
        KLOG_ERR("selftest", "Block test 4 failed: read at sector_count was not rejected");
        return false;
    }

    res = block_write(bdev, bdev->sector_count - 1, 2, write_buf);
    if (res == 0) {
        KLOG_ERR("selftest", "Block test 4 failed: write spanning past end was not rejected");
        return false;
    }

    /* 5. Read-only protection check */
    bdev->read_only = true;
    res = block_write(bdev, 0, 1, write_buf);
    bdev->read_only = false;

    if (res != -2) {
        KLOG_ERR("selftest", "Block test 5 failed: write to read-only device was not rejected with -2");
        return false;
    }

    kprintf("[PASS] block: sector read/write, multi-block I/O, bounds, and read-only checks verified\n");
    return true;
}
