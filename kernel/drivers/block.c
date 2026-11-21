/* ZERO KERNEL - Generic Block Device Subsystem Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/block.h"
#include "drivers/driver.h"
#include "zero/spinlock.h"
#include "zero/log.h"
#include "zero/string.h"
#include "zero/checked.h"

static block_device_t *g_block_devices = NULL;
static size_t g_block_device_count = 0;
static spinlock_t g_block_lock;

void block_init(void)
{
    spinlock_init(&g_block_lock);
    g_block_devices = NULL;
    g_block_device_count = 0;
    KLOG_INFO("block", "Generic block device subsystem initialized");
}

int block_device_register(block_device_t *bdev)
{
    if (!bdev || !bdev->ops || bdev->sector_size == 0)
        return -1;

    spinlock_acquire(&g_block_lock);

    if (g_block_device_count >= BLOCK_MAX_DEVICES) {
        spinlock_release(&g_block_lock);
        KLOG_ERR("block", "Block device registry full; cannot register '%s'", bdev->name);
        return -1;
    }

    bdev->id = (uint32_t)(g_block_device_count + 1);
    bdev->next = g_block_devices;
    g_block_devices = bdev;
    g_block_device_count++;

    /* Link into Driver Manager */
    bdev->dev_entry.type = DEVICE_TYPE_BLOCK;
    bdev->dev_entry.state = DEVICE_STATE_ACTIVE;
    strncpy(bdev->dev_entry.name, bdev->name, sizeof(bdev->dev_entry.name) - 1);

    device_register(&bdev->dev_entry);

    spinlock_release(&g_block_lock);

    uint64_t capacity_kib = (bdev->sector_count * bdev->sector_size) / 1024;
    KLOG_INFO("block", "Registered block device #%u: '%s' (%lu sectors, %lu KiB, sector size: %u)",
              bdev->id, bdev->name, bdev->sector_count, capacity_kib, bdev->sector_size);

    return 0;
}

block_device_t *block_device_get_by_name(const char *name)
{
    if (!name)
        return NULL;

    spinlock_acquire(&g_block_lock);
    block_device_t *curr = g_block_devices;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            spinlock_release(&g_block_lock);
            return curr;
        }
        curr = curr->next;
    }
    spinlock_release(&g_block_lock);
    return NULL;
}

block_device_t *block_device_get(size_t index)
{
    spinlock_acquire(&g_block_lock);
    block_device_t *curr = g_block_devices;
    size_t i = 0;
    while (curr) {
        if (i == index) {
            spinlock_release(&g_block_lock);
            return curr;
        }
        i++;
        curr = curr->next;
    }
    spinlock_release(&g_block_lock);
    return NULL;
}

size_t block_device_count(void)
{
    return g_block_device_count;
}

int block_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer)
{
    if (!dev || !buffer || count == 0)
        return -1;

    if (!dev->ops || !dev->ops->read_blocks)
        return -1;

    uint64_t end_lba;
    if (chk_add_u64(lba, (uint64_t)count, &end_lba))
        return -1;

    if (end_lba > dev->sector_count) {
        KLOG_WARN("block", "'%s' read out of bounds: lba=%lu count=%u > capacity=%lu",
                  dev->name, lba, count, dev->sector_count);
        return -1;
    }

    return dev->ops->read_blocks(dev, lba, count, buffer);
}

int block_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer)
{
    if (!dev || !buffer || count == 0)
        return -1;

    if (dev->read_only) {
        KLOG_WARN("block", "Cannot write to read-only device '%s'", dev->name);
        return -2;
    }

    if (!dev->ops || !dev->ops->write_blocks)
        return -1;

    uint64_t end_lba;
    if (chk_add_u64(lba, (uint64_t)count, &end_lba))
        return -1;

    if (end_lba > dev->sector_count) {
        KLOG_WARN("block", "'%s' write out of bounds: lba=%lu count=%u > capacity=%lu",
                  dev->name, lba, count, dev->sector_count);
        return -1;
    }

    return dev->ops->write_blocks(dev, lba, count, buffer);
}

int block_flush(block_device_t *dev)
{
    if (!dev)
        return -1;

    if (dev->ops && dev->ops->flush)
        return dev->ops->flush(dev);

    return 0;
}

void block_dump_devices(void)
{
    KLOG_INFO("block", "===== ZERO OS STORAGE BLOCK DEVICE INVENTORY (%lu devices) =====", g_block_device_count);
    spinlock_acquire(&g_block_lock);
    block_device_t *curr = g_block_devices;
    while (curr) {
        uint64_t size_mib = (curr->sector_count * curr->sector_size) / (1024 * 1024);
        KLOG_INFO("block", "  Block #%02u: %-16s %6lu MiB (%lu sectors, %u B/sect) [%s]",
                  curr->id, curr->name, size_mib, curr->sector_count, curr->sector_size,
                  curr->read_only ? "RO" : "RW");
        curr = curr->next;
    }
    spinlock_release(&g_block_lock);
    KLOG_INFO("block", "===============================================================");
}
