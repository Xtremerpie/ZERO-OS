# ZEROFS On-Disk Format Specification

**Milestone:** 4.0.0 Storage & Networking  
**Magic Number:** `0x5A45524F` ("ZERO")  
**Block Size:** 4096 bytes  
**Inode Size:** 128 bytes  
**Dirent Size:** 64 bytes  

---

## 1. On-Disk Layout

A ZEROFS formatted partition or disk layout is organized as follows:

| Block Index | Content | Purpose |
| :--- | :--- | :--- |
| **Block 0** | Reserved / Boot Block | Host MBR or boot code reserved region |
| **Block 1** | Superblock | Filesystem geometry, counters, volume name |
| **Block 2** | Inode Allocation Bitmap | 1 bit per inode (0 = free, 1 = allocated) |
| **Block 3** | Block Allocation Bitmap | 1 bit per block (0 = free, 1 = allocated) |
| **Blocks 4..19** | Inode Table | 16 blocks holding 512 128-byte inodes |
| **Blocks 20..** | Data Blocks | File content blocks and directory entry tables |

---

## 2. Superblock Structure (Block 1)

```c
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
    uint32_t inode_table_blocks;  /* 16 */
    uint32_t first_data_block;    /* Block 20 */
    uint32_t root_inode;          /* Inode 1 */
    char     volume_name[32];     /* e.g. "ZERO_ROOT" */
    uint32_t mount_count;
    uint32_t state;               /* 1 = clean, 0 = dirty */
    uint32_t checksum;
} zerofs_super_t;
```

---

## 3. Inode Structure (128 bytes)

```c
typedef struct __attribute__((packed)) zerofs_inode {
    uint16_t mode;        /* type + permissions: 0x8000 (file) or 0x4000 (dir) */
    uint16_t uid;
    uint16_t gid;
    uint16_t nlink;
    uint64_t size;        /* File size in bytes */
    uint32_t blocks;      /* Count of 4096-byte blocks allocated */
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint32_t direct[12];  /* 12 direct block pointers (covers up to 48 KiB) */
    uint32_t indirect_1;  /* Single indirect block (1024 pointers -> up to 4 MiB) */
    uint32_t indirect_2;  /* Double indirect block (up to 4 GiB) */
    uint32_t flags;
    uint32_t reserved[9]; /* Padding to exactly 128 bytes */
} zerofs_inode_t;
```

---

## 4. Directory Entry Structure (64 bytes)

```c
typedef struct __attribute__((packed)) zerofs_dirent {
    uint32_t inode;
    uint16_t rec_len;     /* 64 bytes */
    uint8_t  name_len;
    uint8_t  file_type;   /* 1 = regular file, 2 = directory */
    char     name[56];    /* Null-terminated ASCII filename */
} zerofs_dirent_t;
```

- Each 4096-byte data block allocated to a directory contains exactly $4096 / 64 = 64$ directory entries.
- Unused or unlinked entries have `inode = 0`.
- The root directory (Inode 1) is pre-populated with `.` (Inode 1) and `..` (Inode 1).
