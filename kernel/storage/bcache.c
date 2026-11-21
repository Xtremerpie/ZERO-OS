/* ZERO KERNEL - Block Buffer Cache Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements LRU-managed 4 KiB block cache, write-back/write-through dirty tracking,
 * and reliable persistence synchronization.
 */
#include "storage/bcache.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "lib/string.h"

static bcache_entry_t g_bcache[BCACHE_NUM_ENTRIES];
static bcache_stats_t g_bcache_stats;
static spinlock_t     g_bcache_lock;
static uint64_t       g_bcache_timer = 0;

void bcache_init(void)
{
    spinlock_init(&g_bcache_lock);
    spin_lock(&g_bcache_lock);
    memset(g_bcache, 0, sizeof(g_bcache));
    memset(&g_bcache_stats, 0, sizeof(g_bcache_stats));
    g_bcache_timer = 0;
    spin_unlock(&g_bcache_lock);
    KLOG_INFO("bcache", "initialized 4 KiB block cache (%u entries, %u KiB total)",
              BCACHE_NUM_ENTRIES, (BCACHE_NUM_ENTRIES * BCACHE_BLOCK_SIZE) / 1024u);
}

static int flush_entry_locked(bcache_entry_t *entry)
{
    if (!entry->valid || !entry->dirty || !entry->dev) {
        return 0;
    }

    uint64_t lba = entry->block_num * (BCACHE_BLOCK_SIZE / BLOCK_SECTOR_SIZE);
    uint32_t sectors = BCACHE_BLOCK_SIZE / BLOCK_SECTOR_SIZE;

    int rc = block_write(entry->dev, lba, sectors, entry->data);
    if (rc == 0) {
        entry->dirty = false;
        g_bcache_stats.flushes++;
    } else {
        KLOG_WARN("bcache", "failed to flush block %llu on device %s",
                  (unsigned long long)entry->block_num, entry->dev->name);
    }
    return rc;
}

static bcache_entry_t *find_or_evict_locked(block_device_t *dev, uint64_t block_num)
{
    /* 1. Look for existing entry */
    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (g_bcache[i].valid && g_bcache[i].dev == dev && g_bcache[i].block_num == block_num) {
            return &g_bcache[i];
        }
    }

    /* 2. Look for free entry */
    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (!g_bcache[i].valid) {
            return &g_bcache[i];
        }
    }

    /* 3. Find LRU entry with pin_count == 0 */
    bcache_entry_t *lru_candidate = NULL;
    uint64_t min_age = (uint64_t)-1;

    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (g_bcache[i].pin_count == 0) {
            if (g_bcache[i].lru_counter < min_age) {
                min_age = g_bcache[i].lru_counter;
                lru_candidate = &g_bcache[i];
            }
        }
    }

    if (lru_candidate) {
        /* Evict LRU entry */
        if (lru_candidate->dirty) {
            flush_entry_locked(lru_candidate);
        }
        lru_candidate->valid = false;
        g_bcache_stats.evictions++;
    }

    return lru_candidate;
}

int bcache_read(block_device_t *dev, uint64_t block_num, void *out_buf)
{
    if (!dev || !out_buf) return -1;

    spin_lock(&g_bcache_lock);

    /* Check if already in cache */
    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (g_bcache[i].valid && g_bcache[i].dev == dev && g_bcache[i].block_num == block_num) {
            memcpy(out_buf, g_bcache[i].data, BCACHE_BLOCK_SIZE);
            g_bcache[i].lru_counter = ++g_bcache_timer;
            g_bcache_stats.hits++;
            spin_unlock(&g_bcache_lock);
            return 0;
        }
    }

    g_bcache_stats.misses++;
    bcache_entry_t *entry = find_or_evict_locked(dev, block_num);
    if (!entry) {
        spin_unlock(&g_bcache_lock);
        return -2; /* Cache exhausted and all entries pinned */
    }

    /* Read from disk into cache entry */
    uint64_t lba = block_num * (BCACHE_BLOCK_SIZE / BLOCK_SECTOR_SIZE);
    uint32_t sectors = BCACHE_BLOCK_SIZE / BLOCK_SECTOR_SIZE;

    int rc = block_read(dev, lba, sectors, entry->data);
    if (rc != 0) {
        spin_unlock(&g_bcache_lock);
        return rc;
    }

    entry->dev = dev;
    entry->block_num = block_num;
    entry->valid = true;
    entry->dirty = false;
    entry->lru_counter = ++g_bcache_timer;

    memcpy(out_buf, entry->data, BCACHE_BLOCK_SIZE);
    spin_unlock(&g_bcache_lock);
    return 0;
}

int bcache_write(block_device_t *dev, uint64_t block_num, const void *in_buf, bool write_through)
{
    if (!dev || !in_buf) return -1;
    if (dev->read_only) return -3;

    spin_lock(&g_bcache_lock);

    bcache_entry_t *entry = find_or_evict_locked(dev, block_num);
    if (!entry) {
        spin_unlock(&g_bcache_lock);
        return -2;
    }

    entry->dev = dev;
    entry->block_num = block_num;
    entry->valid = true;
    entry->dirty = true;
    entry->lru_counter = ++g_bcache_timer;
    memcpy(entry->data, in_buf, BCACHE_BLOCK_SIZE);
    g_bcache_stats.writes++;

    if (write_through) {
        flush_entry_locked(entry);
    }

    spin_unlock(&g_bcache_lock);
    return 0;
}

int bcache_sync(block_device_t *dev)
{
    if (!dev) return -1;

    spin_lock(&g_bcache_lock);
    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (g_bcache[i].valid && g_bcache[i].dirty && g_bcache[i].dev == dev) {
            flush_entry_locked(&g_bcache[i]);
        }
    }
    spin_unlock(&g_bcache_lock);

    return block_flush(dev);
}

int bcache_flush_all(void)
{
    spin_lock(&g_bcache_lock);
    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (g_bcache[i].valid && g_bcache[i].dirty) {
            flush_entry_locked(&g_bcache[i]);
        }
    }
    spin_unlock(&g_bcache_lock);
    return 0;
}

void bcache_invalidate(block_device_t *dev)
{
    spin_lock(&g_bcache_lock);
    for (size_t i = 0; i < BCACHE_NUM_ENTRIES; i++) {
        if (g_bcache[i].valid && (dev == NULL || g_bcache[i].dev == dev)) {
            if (g_bcache[i].dirty) {
                flush_entry_locked(&g_bcache[i]);
            }
            g_bcache[i].valid = false;
        }
    }
    spin_unlock(&g_bcache_lock);
}

void bcache_get_stats(bcache_stats_t *out_stats)
{
    if (!out_stats) return;
    spin_lock(&g_bcache_lock);
    memcpy(out_stats, &g_bcache_stats, sizeof(bcache_stats_t));
    spin_unlock(&g_bcache_lock);
}
