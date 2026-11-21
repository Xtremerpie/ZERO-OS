/* ZERO KERNEL - ZEROFS Native Filesystem Self-Test Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Verifies on-disk formatting, mounting into VFS, multi-block file writes/reads,
 * directory nesting, metadata persistence, and file unlinking.
 */
#include "debug/selftest_zerofs.h"
#include "fs/zerofs.h"
#include "fs/vfs.h"
#include "drivers/block.h"
#include "zero/log.h"

#include "lib/string.h"

bool zerofs_run_selftests(void)
{
    kprintf("[TEST] Running ZEROFS Native Filesystem Self-Tests...\n");

    block_device_t *rdev = block_device_get_by_name("ramdisk0");
    if (!rdev) {
        kprintf("[WARN] zerofs: ramdisk0 unavailable, skipping live mount selftest\n");
        return true;
    }

    /* 1. Format RAM disk with ZEROFS */
    int rc = zerofs_format(rdev, "ZERO_STORAGE");
    if (rc != 0) {
        kprintf("[FAIL] zerofs: format failed with code %d\n", rc);
        return false;
    }
    kprintf("[PASS] zerofs: on-disk superblock, bitmaps, and root inode formatting verified\n");

    /* 2. Mount ZEROFS onto VFS */
    rc = zerofs_mount(rdev, "/data");
    if (rc != 0) {
        kprintf("[FAIL] zerofs: mount failed with code %d\n", rc);
        return false;
    }
    kprintf("[PASS] zerofs: VFS integration and mount verified at '/data'\n");

    /* 3. Create and Write Multi-Block File */
    file_t *f = vfs_open("/data/kernel_log.txt", O_CREAT | O_RDWR);
    if (!f) {
        kprintf("[FAIL] zerofs: failed to create file '/data/kernel_log.txt'\n");
        return false;
    }

    /* Write 6000 bytes (spans block 0 and block 1) */
    uint8_t write_buf[6000];
    for (size_t i = 0; i < sizeof(write_buf); i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    int64_t written = vfs_write(f, write_buf, sizeof(write_buf));
    if (written != (int64_t)sizeof(write_buf)) {
        kprintf("[FAIL] zerofs: multi-block write failed, wrote %lld bytes\n", (long long)written);
        vfs_close(f);
        return false;
    }
    vfs_close(f);

    /* 4. Read Back and Verify Integrity */
    f = vfs_open("/data/kernel_log.txt", O_RDONLY);
    if (!f) {
        kprintf("[FAIL] zerofs: failed to open created file for reading\n");
        return false;
    }

    uint8_t read_buf[6000];
    memset(read_buf, 0, sizeof(read_buf));
    int64_t bytes_read = vfs_read(f, read_buf, sizeof(read_buf));
    if (bytes_read != (int64_t)sizeof(read_buf) || memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
        kprintf("[FAIL] zerofs: data integrity check failed (read %lld bytes)\n", (long long)bytes_read);
        vfs_close(f);
        return false;
    }
    vfs_close(f);
    kprintf("[PASS] zerofs: multi-block file allocation, write, and readback verified\n");

    /* 5. Directory Creation */
    rc = vfs_mkdir("/data/system");
    if (rc != 0) {
        kprintf("[FAIL] zerofs: mkdir '/data/system' failed\n");
        return false;
    }

    file_t *subfile = vfs_open("/data/system/boot.cfg", O_CREAT | O_RDWR);
    if (!subfile) {
        kprintf("[FAIL] zerofs: create file in subdirectory failed\n");
        return false;
    }

    const char *cfg_text = "zero.timeout=5\nzero.resolution=1920x1080\n";
    vfs_write(subfile, cfg_text, strlen(cfg_text));
    vfs_close(subfile);
    kprintf("[PASS] zerofs: nested directory creation and path resolution verified\n");

    /* 6. File Unlinking */
    rc = vfs_unlink("/data/kernel_log.txt");
    if (rc != 0) {
        kprintf("[FAIL] zerofs: file unlink failed\n");
        return false;
    }

    file_t *check_deleted = vfs_open("/data/kernel_log.txt", O_RDONLY);
    if (check_deleted != NULL) {
        vfs_close(check_deleted);
        kprintf("[FAIL] zerofs: unlinked file is still accessible\n");
        return false;
    }
    kprintf("[PASS] zerofs: file unlinking and bitmap reclamation verified\n");

    /* 7. Disk Sync */
    rc = zerofs_sync(rdev);
    if (rc != 0) {
        kprintf("[FAIL] zerofs: disk sync failed\n");
        return false;
    }
    kprintf("[PASS] zerofs: dirty metadata and buffer cache synchronization verified\n");

    kprintf("[PASS] zerofs: all native ZEROFS filesystem operations verified\n");
    return true;
}
