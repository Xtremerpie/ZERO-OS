/* ZERO KERNEL - physical memory manager (Version 1.0.0): bitmap frame allocator.
 *
 * Physical addresses are plain uint64_t and are NEVER dereferenced directly; the kernel reaches
 * physical memory only through the HHDM (virtual = physical + hhdm offset).
 * One bit per 4096-byte frame, 1 = used/unavailable, 0 = free. Allocation is first-fit.
 *
 * Never allocatable: everything below 1 MiB (policy), every non-USABLE region (kernel image,
 * bootloader-reclaimable, ACPI, framebuffer, reserved, bad, holes) and the bitmap's own frames.
 *
 * SYNCHRONIZATION: none. 1.0.0 runs on one CPU with interrupts not yet used; the PMM is NOT
 * thread-safe and NOT interrupt-safe. A lock must be added before SMP or IRQ-context use.
 *
 * Two layers: an instance API (struct pmm, usable on synthetic maps by tests) and a global API
 * (the kernel's single PMM). */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "memory/memmap.h"

#define PMM_LOW_LIMIT 0x100000ull /* frames below 1 MiB are never allocated */

enum pmm_status {
    PMM_OK = 0,
    PMM_ERR_NOT_INITIALIZED,
    PMM_ERR_ALREADY_INITIALIZED,
    PMM_ERR_BAD_MAP,
    PMM_ERR_NO_STORAGE,
    PMM_ERR_UNALIGNED,
    PMM_ERR_RANGE,         /* outside any known region, or arithmetic overflow */
    PMM_ERR_PROTECTED,     /* known but never allocatable (reserved, kernel, bitmap, <1 MiB ...) */
    PMM_ERR_NOT_ALLOCATED, /* double free */
    PMM_ERR_COUNT,         /* zero / absurd count */
    PMM_ERR_NO_MEMORY      /* no free run large enough */
};

struct pmm_stats {
    uint64_t total_memory;      /* bytes described by the normalized map (excludes holes) */
    uint64_t usable_memory;     /* bytes of USABLE regions */
    uint64_t reserved_memory;   /* total - usable */
    uint64_t allocatable_memory;/* usable, >= 1 MiB, minus the bitmap itself */
    uint64_t free_memory;
    uint64_t allocated_memory;
    uint64_t total_frames;      /* total_memory / 4096 */
    uint64_t allocatable_frames;
    uint64_t free_frames;
    uint64_t allocated_frames;
    uint64_t peak_allocated_frames;
    uint64_t alloc_calls;
    uint64_t alloc_failures;
    uint64_t free_calls;
    uint64_t invalid_frees;     /* rejected frees of any kind */
    uint64_t double_frees;      /* subset: frame already free */
    uint64_t bitmap_bytes;      /* bitmap size actually needed */
    uint64_t tracked_frames;    /* frames covered by the bitmap (0 .. highest usable end) */
};

struct pmm {
    bool initialized;
    struct memmap map;          /* private copy; used to validate frees */
    uint64_t *bitmap;           /* 1 = used */
    uint64_t words;
    uint64_t frames;            /* tracked frames */
    uint64_t hint;              /* invariant: every frame below hint is used */
    uint64_t prot_first;        /* bitmap storage: first frame ... */
    uint64_t prot_frames;       /* ... and count (0 if storage is external) */
    uint64_t allocatable;
    uint64_t free_frames;
    uint64_t peak_used;
    uint64_t alloc_calls, alloc_failures, free_calls, invalid_frees, double_frees;
};

/* ---- instance API ---- */
void pmm_instance_reset(struct pmm *p);
/* storage: caller memory (virtual pointer) for the bitmap, `storage_words` 64-bit words.
 * prot_base/prot_frames: physical frames occupied by that storage when it lies inside a USABLE
 * region (they are marked used and protected); pass 0/0 for external storage. */
enum pmm_status pmm_instance_init(struct pmm *p, const struct memmap *m, uint64_t *storage,
                                  uint64_t storage_words, uint64_t prot_base, uint64_t prot_frames);
enum pmm_status pmm_instance_alloc(struct pmm *p, uint64_t count, uint64_t *out_addr);
enum pmm_status pmm_instance_free(struct pmm *p, uint64_t addr, uint64_t count);
void pmm_instance_stats(const struct pmm *p, struct pmm_stats *s);
/* Full internal consistency check: recounts the bitmap, padding bits, hint invariant,
 * protected frames. Returns true if consistent. */
bool pmm_instance_verify(const struct pmm *p);
/* Fragmentation: number of free runs and the longest free run (frames). */
void pmm_instance_fragmentation(const struct pmm *p, uint64_t *runs, uint64_t *longest);
/* True if the frame at addr is currently marked used (false for out-of-range). */
bool pmm_instance_frame_used(const struct pmm *p, uint64_t addr);

/* ---- global API (the kernel PMM) ---- */
enum pmm_status pmm_init(const struct memmap *m, uint64_t hhdm_offset);
bool pmm_is_initialized(void);
uint64_t pmm_alloc_page(void);                 /* physical address, 0 on failure */
uint64_t pmm_alloc_pages(uint64_t count);      /* physically contiguous; 0 on failure */
enum pmm_status pmm_free_page(uint64_t addr);
enum pmm_status pmm_free_pages(uint64_t addr, uint64_t count);
void pmm_get_stats(struct pmm_stats *s);
/* Internal access for selftests/diagnostics; NULL before init. */
struct pmm *pmm_global(void);
uint64_t pmm_hhdm(void);
const char *pmm_status_name(enum pmm_status s);
