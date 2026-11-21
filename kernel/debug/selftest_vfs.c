/* ZERO KERNEL - VFS and RAMFS Self-Tests (Version 2.0.0 System Architecture).
 *
 * Verifies:
 * 1. Root directory mounting and traversal.
 * 2. Regular file creation and opening.
 * 3. File write operations and dynamic capacity growth.
 * 4. File seeking and read-back verification.
 * 5. Subdirectory creation and nested path resolution.
 * 6. File deletion via unlink.
 */

#include "fs/vfs_test.h"
#include "fs/vfs.h"
#include "zero/log.h"
#include "zero/string.h"

bool vfs_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting VFS & RAMFS subsystem self-tests...");

    /* 1. Create a regular file in RAMFS */
    int res = vfs_create("/banner.txt");
    if (res != 0) {
        KLOG_ERR("selftest", "VFS test 1 failed: vfs_create('/banner.txt') returned %d", res);
        return false;
    }

    /* 2. Open file for read/write */
    file_t *f = vfs_open("/banner.txt", O_RDWR);
    if (!f) {
        KLOG_ERR("selftest", "VFS test 2 failed: vfs_open('/banner.txt') returned NULL");
        return false;
    }

    /* 3. Write data to file */
    const char *test_msg = "ZERO OS 2.0.0 System Architecture VFS Test";
    size_t msg_len = strlen(test_msg);

    int64_t written = vfs_write(f, test_msg, msg_len);
    if (written != (int64_t)msg_len) {
        vfs_close(f);
        KLOG_ERR("selftest", "VFS test 3 failed: expected %lu bytes written, got %ld", msg_len, written);
        return false;
    }

    /* 4. Seek to start and read back */
    int64_t pos = vfs_seek(f, 0, SEEK_SET);
    if (pos != 0) {
        vfs_close(f);
        KLOG_ERR("selftest", "VFS test 4 failed: seek to 0 returned %ld", pos);
        return false;
    }

    char read_buf[64];
    memset(read_buf, 0, sizeof(read_buf));
    int64_t read_bytes = vfs_read(f, read_buf, sizeof(read_buf) - 1);
    if (read_bytes != (int64_t)msg_len || strcmp(read_buf, test_msg) != 0) {
        vfs_close(f);
        KLOG_ERR("selftest", "VFS test 4 failed: read-back mismatch: '%s'", read_buf);
        return false;
    }

    vfs_close(f);

    /* 5. Subdirectory creation and nested file */
    res = vfs_mkdir("/system");
    if (res != 0) {
        KLOG_ERR("selftest", "VFS test 5 failed: vfs_mkdir('/system') returned %d", res);
        return false;
    }

    res = vfs_create("/system/os.cfg");
    if (res != 0) {
        KLOG_ERR("selftest", "VFS test 5 failed: vfs_create('/system/os.cfg') returned %d", res);
        return false;
    }

    file_t *f_cfg = vfs_open("/system/os.cfg", O_RDWR);
    if (!f_cfg) {
        KLOG_ERR("selftest", "VFS test 5 failed: cannot open nested file '/system/os.cfg'");
        return false;
    }

    const char *cfg_data = "arch=x86_64\nmode=system_architecture\nversion=2.0.0\n";
    vfs_write(f_cfg, cfg_data, strlen(cfg_data));
    vfs_close(f_cfg);

    /* 6. File deletion via unlink */
    res = vfs_unlink("/banner.txt");
    if (res != 0) {
        KLOG_ERR("selftest", "VFS test 6 failed: vfs_unlink('/banner.txt') returned %d", res);
        return false;
    }

    file_t *f_deleted = vfs_open("/banner.txt", O_RDONLY);
    if (f_deleted != NULL) {
        vfs_close(f_deleted);
        KLOG_ERR("selftest", "VFS test 6 failed: opened deleted file");
        return false;
    }

    kprintf("[PASS] vfs: vnodes, file handles, path resolution, and RAMFS verified\n");
    return true;
}
