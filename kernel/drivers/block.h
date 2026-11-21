/* ZERO KERNEL - Generic Block Device Abstraction Interface (Milestone 3.0.0 Hardware Foundation).
 *
 * Defines the standard block device interface, sector I/O operations,
 * bounds validation, error handling, and device registration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/driver.h"

#define BLOCK_MAX_DEVICES 8u
#define BLOCK_SECTOR_SIZE 512u

struct block_device;

typedef struct block_device_ops {
    int (*read_blocks)(struct block_device *dev, uint64_t lba, uint32_t count, void *buffer);
    int (*write_blocks)(struct block_device *dev, uint64_t lba, uint32_t count, const void *buffer);
    int (*flush)(struct block_device *dev);
} block_device_ops_t;

typedef struct block_device {
    char name[32];
    uint32_t id;
    uint64_t sector_count;
    uint32_t sector_size;
    bool read_only;

    const block_device_ops_t *ops;
    void *driver_data;

    device_t dev_entry;
    struct block_device *next;
} block_device_t;

/* Initialize block device management subsystem */
void block_init(void);

/* Register a block device with the subsystem and central driver manager */
int block_device_register(block_device_t *bdev);

/* Retrieve block device by name */
block_device_t *block_device_get_by_name(const char *name);

/* Retrieve block device by index */
block_device_t *block_device_get(size_t index);

/* Total registered block devices */
size_t block_device_count(void);

/* Read sectors with strict parameter, boundary, and alignment checks */
int block_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer);

/* Write sectors with parameter, boundary, and read-only protection checks */
int block_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer);

/* Flush device write caches */
int block_flush(block_device_t *dev);

/* Print all registered block devices and storage capacities */
void block_dump_devices(void);
