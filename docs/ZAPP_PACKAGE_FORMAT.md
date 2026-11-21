# ZERO Application Package Format (.zapp) Specification

**Version:** 1.0.0  
**Magic Signature:** `0x5A415050` (`"ZAPP"` in little-endian)  
**Status:** Official Standard for ZERO OS 6.0.0+

---

## 1. Overview

The `.zapp` file format is the official binary package format of ZERO OS. Designed specifically for freestanding microkernel systems, it provides a self-contained, integrity-verified archive containing application metadata, binary executables, icons, configuration files, and assets.

Unlike generic archive formats (e.g. `.tar.gz`, `.deb`, `.rpm`), `.zapp` requires no runtime decompression library to inspect headers, uses fixed-size structures with power-of-two alignments, and features mandatory path traversal defenses and cryptographic checksums.

---

## 2. Binary Layout

A `.zapp` package consists of four contiguous sections:

```
+-------------------------------------------------------------+
| Header: zapp_header_t (128 bytes)                           |
+-------------------------------------------------------------+
| Manifest: zapp_manifest_t (384 bytes)                       |
+-------------------------------------------------------------+
| File Entry Table: zapp_file_entry_t[N] (144 bytes * N)      |
+-------------------------------------------------------------+
| Raw Payload Data: Byte stream of all archived files         |
+-------------------------------------------------------------+
```

---

## 3. Structure Definitions

### 3.1 Header (`zapp_header_t` — 128 Bytes)

```c
typedef struct zapp_header {
    uint32_t magic;              /* 0x5A415050 ("ZAPP") */
    uint16_t format_version;     /* Version (1) */
    uint16_t flags;              /* Reserved flags */
    char     app_id[32];         /* Unique ID e.g. "zero.calculator" */
    char     app_version[16];    /* Semantic version e.g. "1.0.0" */
    char     target_arch[12];    /* "x86_64" */
    uint32_t manifest_offset;    /* Offset to manifest structure (128) */
    uint32_t manifest_size;      /* Size of manifest (384) */
    uint32_t entry_count;        /* Number of file entries (N) */
    uint32_t entries_offset;     /* Offset to entries table (512) */
    uint32_t payload_offset;     /* Offset to raw payload data */
    uint32_t payload_size;       /* Total size of raw payload */
    uint32_t checksum;           /* 32-bit CRC32 checksum over payload */
    uint8_t  sha256[32];         /* SHA-256 cryptographic digest */
} zapp_header_t;
```

### 3.2 Manifest (`zapp_manifest_t` — 384 Bytes)

```c
typedef struct zapp_manifest {
    char     name[64];           /* Display title: "ZERO Calculator" */
    char     description[128];   /* Package description */
    char     category[32];       /* "Productivity", "Utilities", etc. */
    char     author[32];         /* Author / Organization */
    char     license[16];        /* License identifier: "MIT", "GPL-3.0" */
    char     exec_path[64];      /* Relative path to executable in package */
    char     icon_path[32];      /* Relative path to icon in package */
    uint32_t permissions;        /* Capability bitmask (ZAPP_PERM_*) */
    uint32_t min_os_version;     /* Minimum ZERO OS version (e.g. 0x060000) */
    uint8_t  reserved[8];        /* Alignment padding */
} zapp_manifest_t;
```

### 3.3 File Entry (`zapp_file_entry_t` — 144 Bytes)

```c
typedef struct zapp_file_entry {
    char     path[128];          /* Relative path inside package */
    uint32_t offset;             /* Byte offset relative to payload_offset */
    uint32_t size;               /* File size in bytes */
    uint32_t mode;               /* File mode (e.g. 0755, 0644) */
    uint32_t flags;              /* Entry flags (ZAPP_FILE_EXEC, etc.) */
} zapp_file_entry_t;
```

---

## 4. Permission Flags

Applications specify their required capabilities in `zapp_manifest.permissions`:

| Flag | Value | Description |
| :--- | :--- | :--- |
| `ZAPP_PERM_WINDOW` | `0x01` | Allows creating windows and rendering to surfaces |
| `ZAPP_PERM_STORAGE`| `0x02` | Allows read/write access to isolated storage sandbox |
| `ZAPP_PERM_NETWORK`| `0x04` | Allows opening BSD network sockets |
| `ZAPP_PERM_DEVICES`| `0x08` | Allows direct hardware interaction via VFS nodes |
| `ZAPP_PERM_AUDIO`  | `0x10` | Allows audio output and sampling |

---

## 5. Security & Safety Invariants

1. **Path Traversal Rejection**: Any path containing `..`, leading `/` or `\\`, or drive colons (`C:`) is immediately rejected with `ZAPP_ERR_PATH_TRAVERSAL`.
2. **Bounds Enforcement**: `payload_offset + offset + size` must never exceed total package length.
3. **Dual Verification**: Both CRC-32 and SHA-256 are computed and verified prior to unpacking.
