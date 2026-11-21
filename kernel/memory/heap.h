/* ZERO KERNEL - Kernel Heap Allocator (Version 1.0.0 Core Foundation).
 *
 * Provides dynamic memory allocation with:
 * - 16-byte alignment guarantee.
 * - Segregated size classes (16, 32, 64, 128, 256, 512, 1024, 2048 bytes).
 * - Direct page allocation for large requests (> 2048 bytes).
 * - Magic head canaries and tail canaries to detect buffer overflows and heap corruption.
 * - Double-free detection and invalid-free protection.
 * - Lifetime statistics and peak memory monitoring.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define HEAP_ALIGNMENT 16u

struct heap_stats {
    uint64_t total_allocated_bytes;
    uint64_t active_allocated_bytes;
    uint64_t peak_allocated_bytes;
    uint64_t alloc_calls;
    uint64_t free_calls;
    uint64_t double_frees_prevented;
    uint64_t corruptions_detected;
    uint64_t pages_used;
};

/* Initialize the kernel heap subsystem. Must be called after VMM is active. */
void heap_init(void);

/* Returns true if the heap allocator is initialized and ready. */
bool heap_is_initialized(void);

/* Allocate size bytes of memory, aligned to 16 bytes. Returns NULL on failure. */
void *kmalloc(size_t size);

/* Allocate count * size bytes, zero-initialized. Returns NULL on failure or overflow. */
void *kcalloc(size_t count, size_t size);

/* Reallocate an existing block to new_size bytes. */
void *krealloc(void *ptr, size_t new_size);

/* Free a previously allocated memory block. Panics on double-free or corruption. */
void kfree(void *ptr);

/* Retrieve current heap statistics. */
void heap_get_stats(struct heap_stats *out_stats);

/* Internal validation: checks integrity of all active heap blocks. */
bool heap_verify(void);
