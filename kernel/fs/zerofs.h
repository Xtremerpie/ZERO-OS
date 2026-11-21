/* ZERO KERNEL - ZEROFS Native Filesystem (Milestone 4.0.0 Storage & Networking).
 *
 * Implements persistent on-disk filesystem with superblocks, 128-byte inodes,
 * direct and indirect block pointers, free bitmaps, and directory entries.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/block.h"
#include "fs/vfs.h"

#define ZEROFS_MAGIC           0x5A45524Fu /* "ZERO" */
#define ZEROFS_VERSION         1u
#define ZEROFS_BLOCK_SIZE      4096u
#define ZEROFS_INODE_SIZE      128u
#define ZEROFS_DIRENT_SIZE     64u
#define ZEROFS_NAME_MAX        55u

#define ZEROFS_DIRECT_BLOCKS   12u

#define ZEROFS_TYPE_FILE       0x8000u
#define ZEROFS_TYPE_DIR        0x4000u

#define ZEROFS_ROOT_INODE      1u

/* On-disk Superblock (Block 1) */
typedef struct __attribute__((packed)) zerofs_super {
    uint32_t magic;               /* 0x5A45524F */
    uint32_t version;             /* 1 */
    uint32_t block_size;          /* 4096 */
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
    uint32_t inode_bitmap_lba;    /* Block 2 */
    uint32_t inode_bitmap_blocks; /* 1 */
    uint32_t block_bitmap_lba;    /* Block 3 */
    uint32_t block_bitmap_blocks; /* 1 */
    uint32_t inode_table_lba;     /* Block 4 */
    uint32_t inode_table_blocks;  /* e.g. 32 */
    uint32_t first_data_block;    /* e.g. Block 36 */
    uint32_t root_inode;          /* 1 */
    char     volume_name[32];
    uint32_t mount_count;
    uint32_t state;               /* 1 = clean, 0 = dirty */
    uint32_t checksum;
} zerofs_super_t;

/* On-disk Inode Structure (128 bytes) */
typedef struct __attribute__((packed)) zerofs_inode {
    uint16_t mode;        /* type + permissions */
    uint16_t uid;
    uint16_t gid;
    uint16_t nlink;
    uint64_t size;        /* File size in bytes */
    uint32_t blocks;      /* Count of 4 KiB blocks */
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint32_t direct[ZEROFS_DIRECT_BLOCKS];
    uint32_t indirect_1;  /* Single indirect block (1024 pointers) */
    uint32_t indirect_2;  /* Double indirect block */
    uint32_t flags;
    uint32_t reserved[9];
} zerofs_inode_t;

/* On-disk Directory Entry Structure (64 bytes) */
typedef struct __attribute__((packed)) zerofs_dirent {
    uint32_t inode;
    uint16_t rec_len;     /* 64 */
    uint8_t  name_len;
    uint8_t  file_type;   /* 1 = regular file, 2 = directory */
    char     name[56];    /* Null-terminated */
} zerofs_dirent_t;

/* In-memory filesystem mount state */
typedef struct zerofs_mount_info {
    block_device_t *bdev;
    zerofs_super_t  sb;
    vnode_t        *root_vnode;
} zerofs_mount_info_t;

/* In-memory node data attached to vnode_t->fs_data */
typedef struct zerofs_node {
    zerofs_mount_info_t *mnt;
    uint32_t             inode_num;
    zerofs_inode_t       inode;
} zerofs_node_t;

/* Format a block device with a new ZEROFS filesystem */
int zerofs_format(block_device_t *bdev, const char *volume_name);

/* Mount a formatted ZEROFS block device onto a VFS path */
int zerofs_mount(block_device_t *bdev, const char *mount_point);

/* Sync all dirty ZEROFS metadata and blocks to storage */
int zerofs_sync(block_device_t *bdev);
