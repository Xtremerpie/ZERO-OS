/* ZERO KERNEL - Partition Table Parser & Device Creator (Milestone 4.0.0 Storage & Networking).
 *
 * Implements MBR, Protective MBR, and GPT parsing with CRC32 checks,
 * integer overflow protection, and partition block device registration.
 */
#include "storage/partition.h"
#include "zero/log.h"
#include "memory/heap.h"
#include "lib/string.h"

/* Standard IEEE 802.3 CRC32 lookup table (polynomial 0xEDB88320) */
static const uint32_t crc32_table[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
    0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
    0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

uint32_t partition_crc32(const void *data, size_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = p[i];
        crc = (crc >> 4) ^ crc32_table[(crc ^ (byte & 0x0F)) & 0x0F];
        crc = (crc >> 4) ^ crc32_table[(crc ^ (byte >> 4)) & 0x0F];
    }
    return ~crc;
}

bool mbr_is_valid(const mbr_sector_t *mbr)
{
    if (!mbr) return false;
    return (mbr->signature == MBR_SIGNATURE);
}

bool mbr_is_protective_gpt(const mbr_sector_t *mbr)
{
    if (!mbr_is_valid(mbr)) return false;
    for (size_t i = 0; i < MBR_PARTITION_COUNT; i++) {
        if (mbr->entries[i].partition_type == MBR_TYPE_GPT_PROTECT) {
            return true;
        }
    }
    return false;
}

int partition_parse_mbr(block_device_t *parent_dev, const uint8_t *sector_data,
                        partition_info_t *out_parts, size_t max_parts)
{
    if (!parent_dev || !sector_data || !out_parts || max_parts == 0) {
        return -1;
    }

    const mbr_sector_t *mbr = (const mbr_sector_t *)sector_data;
    if (!mbr_is_valid(mbr)) {
        return -2; /* Invalid MBR signature */
    }

    size_t count = 0;
    for (size_t i = 0; i < MBR_PARTITION_COUNT && count < max_parts; i++) {
        const mbr_entry_t *entry = &mbr->entries[i];

        if (entry->partition_type == MBR_TYPE_EMPTY) {
            continue;
        }

        uint64_t start = entry->start_lba;
        uint64_t sectors = entry->sector_count;

        if (sectors == 0) {
            continue;
        }

        /* Integer overflow check */
        if (start + sectors < start) {
            KLOG_WARN("mbr", "partition %u integer overflow on LBA range", (uint32_t)i);
            continue;
        }

        /* Parent device boundary check */
        if (parent_dev->sector_count > 0 && (start + sectors > parent_dev->sector_count)) {
            KLOG_WARN("mbr", "partition %u exceeds device boundary (%llu > %llu)",
                      (uint32_t)i, (unsigned long long)(start + sectors),
                      (unsigned long long)parent_dev->sector_count);
            continue;
        }

        partition_info_t *p = &out_parts[count];
        memset(p, 0, sizeof(*p));
        p->index = (uint32_t)(i + 1);
        p->is_gpt = false;
        p->start_lba = start;
        p->sector_count = sectors;
        p->type_mbr = entry->partition_type;
        p->bootable = (entry->boot_indicator == 0x80);

        /* Format partition name: e.g. "devname_p1" */
        size_t dlen = strlen(parent_dev->name);
        if (dlen > 24) dlen = 24;
        memcpy(p->name, parent_dev->name, dlen);
        p->name[dlen] = 'p';
        p->name[dlen + 1] = (char)('1' + i);
        p->name[dlen + 2] = '\0';

        count++;
    }

    return (int)count;
}

int partition_parse_gpt(block_device_t *parent_dev,
                        partition_info_t *out_parts, size_t max_parts)
{
    if (!parent_dev || !out_parts || max_parts == 0) {
        return -1;
    }

    /* Read LBA 1: GPT Header */
    uint8_t hdr_buf[512];
    int err = block_read(parent_dev, 1, 1, hdr_buf);
    if (err != 0) {
        return -2;
    }

    gpt_header_t *gpt = (gpt_header_t *)hdr_buf;
    if (gpt->signature != GPT_SIGNATURE) {
        return -3; /* Not a valid GPT signature */
    }

    if (gpt->header_size < GPT_MIN_HEADER_SIZE || gpt->header_size > 512) {
        return -4; /* Invalid header size */
    }

    /* Validate Header CRC32 */
    uint32_t original_crc = gpt->header_crc32;
    gpt->header_crc32 = 0;
    uint32_t computed_crc = partition_crc32(gpt, gpt->header_size);
    gpt->header_crc32 = original_crc;

    if (computed_crc != original_crc) {
        KLOG_WARN("gpt", "header CRC32 mismatch: expected 0x%08X, got 0x%08X",
                  original_crc, computed_crc);
        return -5;
    }

    uint32_t num_entries = gpt->num_partition_entries;
    uint32_t entry_size = gpt->sizeof_partition_entry;
    if (entry_size < sizeof(gpt_entry_t) || entry_size > 512) {
        return -6;
    }
    if (num_entries > GPT_MAX_PARTITIONS) {
        num_entries = GPT_MAX_PARTITIONS;
    }

    /* Calculate sectors needed for partition array */
    uint32_t total_array_bytes = num_entries * entry_size;
    uint32_t array_sectors = (total_array_bytes + BLOCK_SECTOR_SIZE - 1) / BLOCK_SECTOR_SIZE;

    uint8_t *array_buf = (uint8_t *)kmalloc(array_sectors * BLOCK_SECTOR_SIZE);
    if (!array_buf) {
        return -7;
    }

    err = block_read(parent_dev, gpt->partition_entry_lba, array_sectors, array_buf);
    if (err != 0) {
        kfree(array_buf);
        return -8;
    }

    /* Verify Partition Array CRC32 */
    uint32_t computed_array_crc = partition_crc32(array_buf, total_array_bytes);
    if (computed_array_crc != gpt->partition_entry_crc32) {
        KLOG_WARN("gpt", "partition array CRC32 mismatch: expected 0x%08X, got 0x%08X",
                  gpt->partition_entry_crc32, computed_array_crc);
        kfree(array_buf);
        return -9;
    }

    size_t count = 0;
    for (uint32_t i = 0; i < num_entries && count < max_parts; i++) {
        const gpt_entry_t *entry = (const gpt_entry_t *)(array_buf + (i * entry_size));

        /* Check for empty/unused GUID */
        bool is_empty = true;
        for (size_t g = 0; g < 16; g++) {
            if (entry->type_guid[g] != 0) {
                is_empty = false;
                break;
            }
        }
        if (is_empty) {
            continue;
        }

        uint64_t start = entry->starting_lba;
        uint64_t end = entry->ending_lba;

        if (start > end) {
            KLOG_WARN("gpt", "partition %u invalid range (start %llu > end %llu)",
                      i, (unsigned long long)start, (unsigned long long)end);
            continue;
        }

        uint64_t sectors = (end - start) + 1;
        if (start + sectors < start) {
            KLOG_WARN("gpt", "partition %u integer overflow", i);
            continue;
        }

        if (parent_dev->sector_count > 0 && (start + sectors > parent_dev->sector_count)) {
            KLOG_WARN("gpt", "partition %u exceeds parent disk boundary", i);
            continue;
        }

        partition_info_t *p = &out_parts[count];
        memset(p, 0, sizeof(*p));
        p->index = i + 1;
        p->is_gpt = true;
        p->start_lba = start;
        p->sector_count = sectors;
        memcpy(p->type_guid, entry->type_guid, 16);
        p->bootable = false;

        /* Extract UTF-16LE partition name to ASCII */
        size_t nidx = 0;
        for (; nidx < 31 && nidx < 36; nidx++) {
            uint16_t ch = entry->name[nidx];
            if (ch == 0) break;
            p->name[nidx] = (ch < 128) ? (char)ch : '?';
        }
        p->name[nidx] = '\0';

        if (nidx == 0) {
            /* Default name */
            size_t dlen = strlen(parent_dev->name);
            if (dlen > 24) dlen = 24;
            memcpy(p->name, parent_dev->name, dlen);
            p->name[dlen] = 'p';
            p->name[dlen + 1] = (char)('1' + (i % 9));
            p->name[dlen + 2] = '\0';
        }

        count++;
    }

    kfree(array_buf);
    return (int)count;
}

/* Partition block device driver private data */
typedef struct part_dev_data {
    block_device_t *parent;
    uint64_t start_lba;
} part_dev_data_t;

static int part_read_blocks(struct block_device *dev, uint64_t lba, uint32_t count, void *buffer)
{
    if (!dev || !dev->driver_data) return -1;
    part_dev_data_t *data = (part_dev_data_t *)dev->driver_data;
    if (lba + count < lba || lba + count > dev->sector_count) return -2;
    return block_read(data->parent, data->start_lba + lba, count, buffer);
}

static int part_write_blocks(struct block_device *dev, uint64_t lba, uint32_t count, const void *buffer)
{
    if (!dev || !dev->driver_data) return -1;
    if (dev->read_only) return -3;
    part_dev_data_t *data = (part_dev_data_t *)dev->driver_data;
    if (lba + count < lba || lba + count > dev->sector_count) return -2;
    return block_write(data->parent, data->start_lba + lba, count, buffer);
}

static int part_flush(struct block_device *dev)
{
    if (!dev || !dev->driver_data) return -1;
    part_dev_data_t *data = (part_dev_data_t *)dev->driver_data;
    return block_flush(data->parent);
}

static const block_device_ops_t part_ops = {
    .read_blocks = part_read_blocks,
    .write_blocks = part_write_blocks,
    .flush = part_flush,
};

block_device_t *partition_create_device(block_device_t *parent_dev, const partition_info_t *part)
{
    if (!parent_dev || !part) return NULL;

    block_device_t *dev = (block_device_t *)kmalloc(sizeof(block_device_t));
    if (!dev) return NULL;
    memset(dev, 0, sizeof(*dev));

    part_dev_data_t *data = (part_dev_data_t *)kmalloc(sizeof(part_dev_data_t));
    if (!data) {
        kfree(dev);
        return NULL;
    }
    data->parent = parent_dev;
    data->start_lba = part->start_lba;

    strncpy(dev->name, part->name, sizeof(dev->name) - 1);
    dev->sector_count = part->sector_count;
    dev->sector_size = parent_dev->sector_size;
    dev->read_only = parent_dev->read_only;
    dev->ops = &part_ops;
    dev->driver_data = data;

    if (block_device_register(dev) != 0) {
        kfree(data);
        kfree(dev);
        return NULL;
    }

    KLOG_INFO("partition", "registered partition device '%s' (LBA %llu..%llu, %llu sectors)",
              dev->name, (unsigned long long)part->start_lba,
              (unsigned long long)(part->start_lba + part->sector_count - 1),
              (unsigned long long)part->sector_count);
    return dev;
}
