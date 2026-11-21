/* ZERO KERNEL - Partition Table Parser & Manager (Milestone 4.0.0 Storage & Networking).
 *
 * Implements MBR (Master Boot Record) and GPT (GUID Partition Table) parsing,
 * CRC32 validation, boundary/overflow verification, and partition block device registration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/block.h"

#define MBR_SIGNATURE          0xAA55u
#define MBR_PARTITION_COUNT    4u
#define MBR_TYPE_EMPTY         0x00u
#define MBR_TYPE_FAT32         0x0Cu
#define MBR_TYPE_ZEROFS        0x83u /* Standard Linux / ZERO native filesystem */
#define MBR_TYPE_GPT_PROTECT   0xEEu
#define MBR_TYPE_EFI_SYSTEM    0xEFu

#define GPT_SIGNATURE          0x5452415020494645ULL /* "EFI PART" */
#define GPT_REVISION_1_0       0x00010000u
#define GPT_MIN_HEADER_SIZE    92u
#define GPT_MAX_PARTITIONS     128u

/* Standard MBR Partition Entry (16 bytes) */
typedef struct __attribute__((packed)) mbr_entry {
    uint8_t  boot_indicator; /* 0x80 = active/bootable, 0x00 = inactive */
    uint8_t  start_chs[3];
    uint8_t  partition_type;
    uint8_t  end_chs[3];
    uint32_t start_lba;
    uint32_t sector_count;
} mbr_entry_t;

/* Standard MBR Sector (512 bytes) */
typedef struct __attribute__((packed)) mbr_sector {
    uint8_t     bootstrap[446];
    mbr_entry_t entries[MBR_PARTITION_COUNT];
    uint16_t    signature;   /* 0xAA55 */
} mbr_sector_t;

/* Standard GPT Header (92+ bytes) */
typedef struct __attribute__((packed)) gpt_header {
    uint64_t signature;           /* "EFI PART" */
    uint32_t revision;            /* 0x00010000 */
    uint32_t header_size;         /* >= 92 bytes */
    uint32_t header_crc32;        /* CRC32 of header with this field set to 0 */
    uint32_t reserved0;
    uint64_t current_lba;         /* Usually LBA 1 */
    uint64_t backup_lba;          /* Last LBA on disk */
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba; /* Usually LBA 2 */
    uint32_t num_partition_entries;
    uint32_t sizeof_partition_entry; /* Usually 128 bytes */
    uint32_t partition_entry_crc32;
} gpt_header_t;

/* Standard GPT Partition Entry (128 bytes) */
typedef struct __attribute__((packed)) gpt_entry {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    uint16_t name[36];            /* UTF-16LE partition name */
} gpt_entry_t;

/* Partition Information Record */
typedef struct partition_info {
    char name[32];
    uint32_t index;
    bool is_gpt;
    uint64_t start_lba;
    uint64_t sector_count;
    uint8_t  type_mbr;
    uint8_t  type_guid[16];
    bool bootable;
} partition_info_t;

/* Compute standard IEEE 802.3 CRC32 */
uint32_t partition_crc32(const void *data, size_t length);

/* Check if MBR sector contains a valid signature */
bool mbr_is_valid(const mbr_sector_t *mbr);

/* Check if MBR contains a Protective GPT partition (type 0xEE) */
bool mbr_is_protective_gpt(const mbr_sector_t *mbr);

/* Parse MBR partition table from 512-byte buffer.
 * Returns number of valid partitions found (up to 4), or negative error code. */
int partition_parse_mbr(block_device_t *parent_dev, const uint8_t *sector_data,
                        partition_info_t *out_parts, size_t max_parts);

/* Parse GPT header and partition entries.
 * Returns number of valid partitions found, or negative error code. */
int partition_parse_gpt(block_device_t *parent_dev,
                        partition_info_t *out_parts, size_t max_parts);

/* Create and register a virtual block device representing a partition */
block_device_t *partition_create_device(block_device_t *parent_dev, const partition_info_t *part);
