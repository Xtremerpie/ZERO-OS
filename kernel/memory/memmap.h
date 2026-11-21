/* ZERO KERNEL - physical memory-map engine (Version 1.0.0).
 *
 * Turns a bootloader memory map into an internal NORMALIZED map: sorted, non-overlapping,
 * page-aligned, adjacent same-type regions merged. Independent of Limine: the adapter in
 * memmap_limine.c converts bootloader entries to struct mem_raw_entry first.
 *
 * Classification rules (docs/MEMORY_MANAGEMENT.md):
 *  - Raw entries must not overlap and must not wrap/exceed 2^52 -> otherwise the whole map is rejected.
 *  - USABLE regions shrink INWARD to 4 KiB boundaries (a partial page is never handed out).
 *  - Every other type expands OUTWARD to 4 KiB boundaries (a partial page is never treated as free).
 *  - A page claimed by several regions takes the highest-priority type:
 *      KERNEL > FRAMEBUFFER > ACPI_NVS > RESERVED = OTHER > BAD > ACPI_RECLAIMABLE
 *      > BOOTLOADER_RECLAIMABLE > USABLE.
 *  - Address space not described by any entry is a HOLE: not in the table, never allocatable. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SIZE  4096ull
#define PAGE_SHIFT 12

#define MEMMAP_MAX_RAW     128u
#define MEMMAP_MAX_REGIONS 128u
#define MEMMAP_MAX_ADDR    (1ull << 52) /* architectural physical-address limit we accept */

enum mem_type {
    MEM_USABLE = 0,
    MEM_RESERVED,
    MEM_ACPI_RECLAIMABLE,
    MEM_ACPI_NVS,
    MEM_BAD,
    MEM_BOOTLOADER_RECLAIMABLE,
    MEM_KERNEL,      /* kernel image and boot modules */
    MEM_FRAMEBUFFER,
    MEM_OTHER,       /* unknown / mapped-reserved / anything unclassified */
    MEM_TYPE_COUNT
};

enum memmap_status {
    MEMMAP_OK = 0,
    MEMMAP_ERR_NULL,     /* null pointer / null entry */
    MEMMAP_ERR_COUNT,    /* zero or too many entries */
    MEMMAP_ERR_OVERFLOW, /* base + length wraps */
    MEMMAP_ERR_RANGE,    /* beyond 2^52 or invalid type */
    MEMMAP_ERR_OVERLAP,  /* two raw entries overlap */
    MEMMAP_ERR_TOO_MANY, /* normalized map would exceed MEMMAP_MAX_REGIONS */
    MEMMAP_ERR_NO_USABLE /* nothing usable after normalization */
};

struct mem_raw_entry {
    uint64_t base;
    uint64_t length;
    enum mem_type type;
};

struct mem_region {
    uint64_t base; /* page aligned */
    uint64_t end;  /* exclusive, page aligned */
    enum mem_type type;
};

struct memmap {
    uint32_t count;
    struct mem_region r[MEMMAP_MAX_REGIONS];
    uint64_t bytes[MEM_TYPE_COUNT]; /* page-granular bytes per type after normalization */
    uint64_t usable_end;            /* end of the highest USABLE region */
};

enum memmap_status memmap_build(struct memmap *out, const struct mem_raw_entry *in, uint64_t n);

/* Region containing addr, or NULL (hole). */
const struct mem_region *memmap_find(const struct memmap *m, uint64_t addr);

const char *memmap_type_name(enum mem_type t);
const char *memmap_status_name(enum memmap_status s);

/* Limine adapter (memmap_limine.c). */
struct limine_memmap_response;
enum memmap_status memmap_raw_from_limine(const struct limine_memmap_response *resp,
                                          struct mem_raw_entry *out, uint64_t cap, uint64_t *n);
