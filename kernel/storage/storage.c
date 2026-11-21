/* ZERO KERNEL - Central Storage Management Subsystem (Milestone 4.0.0 Storage & Networking).
 *
 * Implements high-level storage coordinator, partition auto-discovery,
 * I/O statistics tracking, and device operations.
 */
#include "storage/storage.h"
#include "storage/partition.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "lib/string.h"

static storage_stats_t g_storage_stats;
static spinlock_t g_storage_lock;

void storage_init(void)
{
    spinlock_init(&g_storage_lock);
    storage_reset_stats();
    KLOG_INFO("storage", "central storage subsystem initialized");
}

void storage_reset_stats(void)
{
    spin_lock(&g_storage_lock);
    memset(&g_storage_stats, 0, sizeof(g_storage_stats));
    spin_unlock(&g_storage_lock);
}

void storage_get_stats(storage_stats_t *out_stats)
{
    if (!out_stats) return;
    spin_lock(&g_storage_lock);
    memcpy(out_stats, &g_storage_stats, sizeof(storage_stats_t));
    spin_unlock(&g_storage_lock);
}

int storage_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buf)
{
    if (!dev || !buf || count == 0) {
        spin_lock(&g_storage_lock);
        g_storage_stats.io_errors++;
        spin_unlock(&g_storage_lock);
        return -1;
    }

    int rc = block_read(dev, lba, count, buf);
    spin_lock(&g_storage_lock);
    if (rc == 0) {
        g_storage_stats.reads_completed++;
        g_storage_stats.sectors_read += count;
    } else {
        g_storage_stats.io_errors++;
    }
    spin_unlock(&g_storage_lock);
    return rc;
}

int storage_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buf)
{
    if (!dev || !buf || count == 0) {
        spin_lock(&g_storage_lock);
        g_storage_stats.io_errors++;
        spin_unlock(&g_storage_lock);
        return -1;
    }

    int rc = block_write(dev, lba, count, buf);
    spin_lock(&g_storage_lock);
    if (rc == 0) {
        g_storage_stats.writes_completed++;
        g_storage_stats.sectors_written += count;
    } else {
        g_storage_stats.io_errors++;
    }
    spin_unlock(&g_storage_lock);
    return rc;
}

int storage_flush(block_device_t *dev)
{
    if (!dev) return -1;
    return block_flush(dev);
}

int storage_scan_partitions(block_device_t *dev)
{
    if (!dev || dev->sector_count < 2) {
        return 0;
    }

    /* Don't scan devices that are themselves partitions */
    if (strstr(dev->name, "p1") || strstr(dev->name, "p2") ||
        strstr(dev->name, "p3") || strstr(dev->name, "p4")) {
        return 0;
    }

    uint8_t mbr_buf[512];
    int err = block_read(dev, 0, 1, mbr_buf);
    if (err != 0) {
        KLOG_WARN("storage", "failed to read LBA 0 on device %s", dev->name);
        return -1;
    }

    const mbr_sector_t *mbr = (const mbr_sector_t *)mbr_buf;
    if (!mbr_is_valid(mbr)) {
        /* No standard partition table detected (raw unpartitioned disk or custom FS) */
        return 0;
    }

    partition_info_t parts[16];
    int num_parts = 0;

    if (mbr_is_protective_gpt(mbr)) {
        KLOG_INFO("storage", "%s: protective MBR found, probing GPT...", dev->name);
        num_parts = partition_parse_gpt(dev, parts, 16);
    }

    if (num_parts <= 0) {
        /* Fall back to standard MBR */
        num_parts = partition_parse_mbr(dev, mbr_buf, parts, 16);
    }

    if (num_parts > 0) {
        KLOG_INFO("storage", "%s: discovered %d valid partition(s)", dev->name, num_parts);
        for (int i = 0; i < num_parts; i++) {
            partition_create_device(dev, &parts[i]);
        }
    }

    return num_parts;
}

void storage_scan_all_devices(void)
{
    size_t count = block_device_count();
    for (size_t i = 0; i < count; i++) {
        block_device_t *dev = block_device_get(i);
        if (dev) {
            storage_scan_partitions(dev);
        }
    }
}
