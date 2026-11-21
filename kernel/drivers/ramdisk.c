/* ZERO KERNEL - In-Memory RAM Disk Block Device Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/ramdisk.h"
#include "drivers/block.h"
#include "memory/heap.h"
#include "zero/log.h"
#include "zero/string.h"

static block_device_t g_ramdisk_dev;
static block_device_ops_t g_ramdisk_ops;
static uint8_t *g_ramdisk_storage = NULL;

static int ramdisk_op_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer)
{
    if (!dev || !buffer || !g_ramdisk_storage)
        return -1;

    uint64_t offset = lba * dev->sector_size;
    size_t bytes = (size_t)count * dev->sector_size;

    memcpy(buffer, g_ramdisk_storage + offset, bytes);
    return 0;
}

static int ramdisk_op_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer)
{
    if (!dev || !buffer || !g_ramdisk_storage)
        return -1;

    uint64_t offset = lba * dev->sector_size;
    size_t bytes = (size_t)count * dev->sector_size;

    memcpy(g_ramdisk_storage + offset, buffer, bytes);
    return 0;
}

static int ramdisk_op_flush(block_device_t *dev)
{
    (void)dev;
    return 0; /* Memory writes are immediate */
}

int ramdisk_init(size_t sector_count)
{
    if (sector_count == 0) {
        sector_count = 2048; /* Default 1 MiB (2048 * 512 bytes) */
    }

    size_t total_bytes = sector_count * BLOCK_SECTOR_SIZE;
    g_ramdisk_storage = (uint8_t *)kmalloc(total_bytes);
    if (!g_ramdisk_storage) {
        KLOG_ERR("ramdisk", "Failed to allocate %lu bytes for RAM disk storage", total_bytes);
        return -1;
    }

    memset(g_ramdisk_storage, 0, total_bytes);

    g_ramdisk_ops.read_blocks = ramdisk_op_read;
    g_ramdisk_ops.write_blocks = ramdisk_op_write;
    g_ramdisk_ops.flush = ramdisk_op_flush;

    memset(&g_ramdisk_dev, 0, sizeof(block_device_t));
    strcpy(g_ramdisk_dev.name, "ramdisk0");
    g_ramdisk_dev.sector_count = sector_count;
    g_ramdisk_dev.sector_size = BLOCK_SECTOR_SIZE;
    g_ramdisk_dev.read_only = false;
    g_ramdisk_dev.ops = &g_ramdisk_ops;
    g_ramdisk_dev.driver_data = g_ramdisk_storage;

    int res = block_device_register(&g_ramdisk_dev);
    if (res != 0) {
        kfree(g_ramdisk_storage);
        g_ramdisk_storage = NULL;
        return res;
    }

    KLOG_INFO("ramdisk", "RAM disk initialized: 'ramdisk0' (%lu sectors, %lu KiB)",
              sector_count, total_bytes / 1024);
    return 0;
}

block_device_t *ramdisk_get_device(void)
{
    return g_ramdisk_storage ? &g_ramdisk_dev : NULL;
}
