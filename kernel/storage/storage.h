/* ZERO KERNEL - Central Storage Subsystem (Milestone 4.0.0 Storage & Networking).
 *
 * Manages physical and logical block devices, coordinates partition discovery,
 * tracks I/O statistics, and enforces bounds and alignment on sector operations.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/block.h"

#define STORAGE_MAX_DEVICES 16u

typedef struct storage_stats {
    uint64_t reads_completed;
    uint64_t writes_completed;
    uint64_t sectors_read;
    uint64_t sectors_written;
    uint64_t io_errors;
} storage_stats_t;

/* Initialize the central storage manager */
void storage_init(void);

/* Read sectors through the storage layer with stats and validation */
int storage_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buf);

/* Write sectors through the storage layer with stats and validation */
int storage_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buf);

/* Flush dirty data on device */
int storage_flush(block_device_t *dev);

/* Scan block device for MBR and GPT partitions and register partition devices */
int storage_scan_partitions(block_device_t *dev);

/* Retrieve global storage subsystem statistics */
void storage_get_stats(storage_stats_t *out_stats);

/* Reset storage statistics */
void storage_reset_stats(void);

/* Scan all registered block devices for partitions */
void storage_scan_all_devices(void);
