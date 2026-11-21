/* ZERO KERNEL - Native Application Package Format (.zapp) (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Defines the binary specification, header structures, manifest format,
 * path traversal safety guards, and integrity verification routines.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ZAPP_MAGIC          0x5A415050u /* "ZAPP" in little-endian */
#define ZAPP_FORMAT_VER     1u
#define ZAPP_MAX_PATH       128u
#define ZAPP_MAX_ENTRIES    32u

/* Permission flags */
#define ZAPP_PERM_WINDOW    (1u << 0)
#define ZAPP_PERM_STORAGE   (1u << 1)
#define ZAPP_PERM_NETWORK   (1u << 2)
#define ZAPP_PERM_DEVICES   (1u << 3)
#define ZAPP_PERM_AUDIO     (1u << 4)

/* File entry flags */
#define ZAPP_FILE_EXEC      (1u << 0)
#define ZAPP_FILE_ICON      (1u << 1)
#define ZAPP_FILE_CONFIG    (1u << 2)
#define ZAPP_FILE_RESOURCE  (1u << 3)

#pragma pack(push, 1)

/* 128-byte ZAPP Binary Package Header */
typedef struct zapp_header {
    uint32_t magic;              /* 0x5A415050 ("ZAPP") */
    uint16_t format_version;     /* 1 */
    uint16_t flags;              /* Reserved flags */
    char     app_id[32];         /* Unique ID e.g. "zero.calculator" */
    char     app_version[16];    /* Semantic version e.g. "1.0.0" */
    char     target_arch[12];    /* "x86_64" */
    uint32_t manifest_offset;    /* Offset to manifest structure */
    uint32_t manifest_size;      /* Size of manifest */
    uint32_t entry_count;        /* Number of payload file entries */
    uint32_t entries_offset;     /* Offset to entries array */
    uint32_t payload_offset;     /* Offset to raw payload data */
    uint32_t payload_size;       /* Size of raw payload */
    uint32_t checksum;           /* 32-bit CRC32 checksum over payload */
    uint8_t  sha256[32];         /* SHA256 cryptographic digest */
} zapp_header_t;

/* Manifest Information (384 bytes) */
typedef struct zapp_manifest {
    char     name[64];           /* Display title: "ZERO Calculator" */
    char     description[128];   /* Description */
    char     category[32];       /* "Productivity", "Utilities", etc. */
    char     author[32];         /* Author / Organization */
    char     license[16];        /* License identifier e.g. "MIT" */
    char     exec_path[64];      /* Relative path to executable in package */
    char     icon_path[32];      /* Relative path to icon in package */
    uint32_t permissions;        /* Required system permissions */
    uint32_t min_os_version;     /* Minimum ZERO OS version (0x060000 = 6.0.0) */
    uint8_t  reserved[8];
} zapp_manifest_t;

/* Single File Entry in Payload (144 bytes) */
typedef struct zapp_file_entry {
    char     path[ZAPP_MAX_PATH];/* Relative path inside package */
    uint32_t offset;             /* Byte offset relative to payload_offset */
    uint32_t size;               /* File size in bytes */
    uint32_t mode;               /* File mode permissions (e.g. 0755, 0644) */
    uint32_t flags;              /* ZAPP_FILE_* flags */
} zapp_file_entry_t;

#pragma pack(pop)

/* Error codes for package operations */
typedef enum {
    ZAPP_OK = 0,
    ZAPP_ERR_NULL_PTR,
    ZAPP_ERR_BAD_MAGIC,
    ZAPP_ERR_BAD_VERSION,
    ZAPP_ERR_BAD_ARCH,
    ZAPP_ERR_BAD_SIZE,
    ZAPP_ERR_CORRUPT_PAYLOAD,
    ZAPP_ERR_PATH_TRAVERSAL,
    ZAPP_ERR_DUPLICATE_ENTRY,
    ZAPP_ERR_MISSING_EXEC,
    ZAPP_ERR_CHECKSUM_MISMATCH,
    ZAPP_ERR_OUT_OF_BOUNDS
} zapp_err_t;

/* Validation and safety inspections */
zapp_err_t zapp_validate_path(const char *path);
zapp_err_t zapp_validate_header(const zapp_header_t *hdr, size_t total_size);
zapp_err_t zapp_validate_package(const uint8_t *pkg_bytes, size_t total_size,
                                 zapp_header_t *out_hdr, zapp_manifest_t *out_manifest);

/* Integrity calculation */
uint32_t zapp_calc_checksum(const uint8_t *data, size_t len);

/* Extract a file by path from valid package buffer */
zapp_err_t zapp_get_file(const uint8_t *pkg_bytes, size_t pkg_size,
                         const char *file_path, const uint8_t **out_data, size_t *out_size);

/* Get string representation of error code */
const char *zapp_err_str(zapp_err_t err);
