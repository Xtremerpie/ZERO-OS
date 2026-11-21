/* ZERO KERNEL - Block Buffer Cache (Milestone 4.0.0 Storage & Networking).
 *
 * Provides LRU-evicted caching for 4096-byte filesystem blocks, dirty block
 * tracking, write-back and write-through policies, and disk flush/sync semantics.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/block.h"

#define BCACHE_BLOCK_SIZE   4096u
#define BCACHE_NUM_ENTRIES  64u

typedef struct bcache_entry {
    block_device_t *dev;
    uint64_t        block_num; /* 4 KiB block index (LBA = block_num * 8) */
    uint8_t         data[BCACHE_BLOCK_SIZE];
    bool            valid;
    bool            dirty;
    uint32_t        pin_count;
    uint64_t        lru_counter;
} bcache_entry_t;

typedef struct bcache_stats {
    uint64_t hits;
    uint64_t misses;
    uint64_t writes;
    uint64_t flushes;
    uint64_t evictions;
} bcache_stats_t;

/* Initialize the block cache */
void bcache_init(void);

/* Read a 4 KiB block via the cache (populates cache on miss) */
int bcache_read(block_device_t *dev, uint64_t block_num, void *out_buf);

/* Write a 4 KiB block to the cache (marks dirty or immediately writes through) */
int bcache_write(block_device_t *dev, uint64_t block_num, const void *in_buf, bool write_through);

/* Sync all dirty blocks for a specific device to storage */
int bcache_sync(block_device_t *dev);

/* Flush all dirty blocks across all devices */
int bcache_flush_all(void);

/* Invalidate any cached blocks for a device */
void bcache_invalidate(block_device_t *dev);

/* Retrieve cache performance metrics */
void bcache_get_stats(bcache_stats_t *out_stats);
