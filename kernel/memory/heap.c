/* ZERO KERNEL - Kernel Heap Allocator (Version 1.0.0 Core Foundation).
 *
 * Implements a hybrid allocator:
 * - Small objects (<= 2048 bytes): Segregated free-list size classes (16, 32, 64, 128, 256, 512, 1024, 2048).
 * - Large objects (> 2048 bytes): Direct contiguous page-backed allocations.
 * - Robust security: Header canaries, tail canaries, double-free trap, and alignment checking.
 */

#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"
#include "zero/checked.h"

#define HEAP_MAGIC_ALLOC 0x5A45524F48454150ull /* "ZEROHEAP" */
#define HEAP_MAGIC_FREE  0x465245455A45524Full /* "FREEZERO" */
#define HEAP_CANARY_TAIL 0x43414E4152593437ull /* "CANARY47" */

#define BUCKET_LARGE     0xFFFFFFFFu

/* Size classes for small allocations */
static const size_t BUCKET_SIZES[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
#define NUM_BUCKETS (sizeof(BUCKET_SIZES) / sizeof(BUCKET_SIZES[0]))

struct heap_node {
    struct heap_node *next;
};

struct heap_bucket {
    size_t chunk_size;
    struct heap_node *free_list;
    uint64_t allocations;
    uint64_t frees;
};

struct heap_header {
    uint64_t magic;
    uint64_t req_size;
    uint32_t bucket_idx;
    uint32_t num_pages; /* non-zero for BUCKET_LARGE allocations */
    uint64_t padding;   /* aligns sizeof(struct heap_header) to 32 bytes */
};
_Static_assert(sizeof(struct heap_header) % HEAP_ALIGNMENT == 0, "heap_header must be 16-byte aligned");

static struct heap_bucket buckets[NUM_BUCKETS];
static struct heap_stats stats = { 0 };
static bool initialized = false;

static int get_bucket_index(size_t size)
{
    for (size_t i = 0; i < NUM_BUCKETS; i++) {
        if (size <= BUCKET_SIZES[i])
            return (int)i;
    }
    return -1;
}

void heap_init(void)
{
    for (size_t i = 0; i < NUM_BUCKETS; i++) {
        buckets[i].chunk_size = BUCKET_SIZES[i];
        buckets[i].free_list = NULL;
        buckets[i].allocations = 0;
        buckets[i].frees = 0;
    }
    memset(&stats, 0, sizeof(stats));
    initialized = true;
    KLOG_INFO("heap", "kernel heap allocator initialized (8 size classes: 16B - 2048B)");
}

bool heap_is_initialized(void)
{
    return initialized;
}

void heap_get_stats(struct heap_stats *out_stats)
{
    if (out_stats)
        *out_stats = stats;
}

static void *alloc_from_bucket(int b_idx, size_t req_size)
{
    struct heap_bucket *b = &buckets[b_idx];
    size_t total_chunk = sizeof(struct heap_header) + b->chunk_size + sizeof(uint64_t);

    /* Align total chunk to 16 bytes */
    total_chunk = (total_chunk + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);

    if (!b->free_list) {
        /* Allocate a fresh page to replenish this bucket */
        uint64_t page_phys = pmm_alloc_page();
        if (!page_phys)
            return NULL;

        stats.pages_used++;
        uint8_t *page_virt = (uint8_t *)vmm_phys_to_virt(page_phys);
        size_t count = VMM_PAGE_SIZE / total_chunk;

        for (size_t i = 0; i < count; i++) {
            struct heap_node *node = (struct heap_node *)(page_virt + (i * total_chunk));
            node->next = b->free_list;
            b->free_list = node;
        }
    }

    /* Pop node from free list */
    struct heap_node *node = b->free_list;
    b->free_list = node->next;
    b->allocations++;

    struct heap_header *hdr = (struct heap_header *)node;
    hdr->magic = HEAP_MAGIC_ALLOC;
    hdr->req_size = req_size;
    hdr->bucket_idx = (uint32_t)b_idx;
    hdr->num_pages = 0;

    void *user_ptr = (void *)((char *)hdr + sizeof(struct heap_header));

    /* Write tail canary immediately after requested bytes */
    uint64_t *canary_ptr = (uint64_t *)((char *)user_ptr + req_size);
    *canary_ptr = HEAP_CANARY_TAIL;

    stats.total_allocated_bytes += req_size;
    stats.active_allocated_bytes += req_size;
    if (stats.active_allocated_bytes > stats.peak_allocated_bytes)
        stats.peak_allocated_bytes = stats.active_allocated_bytes;
    stats.alloc_calls++;

    return user_ptr;
}

static void *alloc_large(size_t req_size)
{
    size_t needed = sizeof(struct heap_header) + req_size + sizeof(uint64_t);
    uint64_t pages = (needed + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE;

    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys)
        return NULL;

    stats.pages_used += pages;
    struct heap_header *hdr = (struct heap_header *)vmm_phys_to_virt(phys);
    hdr->magic = HEAP_MAGIC_ALLOC;
    hdr->req_size = req_size;
    hdr->bucket_idx = BUCKET_LARGE;
    hdr->num_pages = (uint32_t)pages;

    void *user_ptr = (void *)((char *)hdr + sizeof(struct heap_header));

    /* Write tail canary */
    uint64_t *canary_ptr = (uint64_t *)((char *)user_ptr + req_size);
    *canary_ptr = HEAP_CANARY_TAIL;

    stats.total_allocated_bytes += req_size;
    stats.active_allocated_bytes += req_size;
    if (stats.active_allocated_bytes > stats.peak_allocated_bytes)
        stats.peak_allocated_bytes = stats.active_allocated_bytes;
    stats.alloc_calls++;

    return user_ptr;
}

void *kmalloc(size_t size)
{
    if (!initialized || size == 0)
        return NULL;

    int b_idx = get_bucket_index(size);
    if (b_idx >= 0) {
        return alloc_from_bucket(b_idx, size);
    } else {
        return alloc_large(size);
    }
}

void *kcalloc(size_t count, size_t size)
{
    uint64_t total;
    if (chk_mul_u64(count, size, &total))
        return NULL;

    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    /* Verify 16-byte alignment of user pointer */
    if (((uintptr_t)ptr & (HEAP_ALIGNMENT - 1)) != 0) {
        kpanic("heap: kfree called with unaligned pointer %p", ptr);
    }

    struct heap_header *hdr = (struct heap_header *)((char *)ptr - sizeof(struct heap_header));

    /* Check for double-free */
    if (hdr->magic == HEAP_MAGIC_FREE) {
        stats.double_frees_prevented++;
        kpanic("heap: DOUBLE-FREE detected at %p (size %llu)", ptr, hdr->req_size);
    }

    /* Verify header magic */
    if (hdr->magic != HEAP_MAGIC_ALLOC) {
        stats.corruptions_detected++;
        kpanic("heap: HEADER CORRUPTION detected at %p (magic 0x%016llx)", ptr, hdr->magic);
    }

    /* Verify tail canary */
    uint64_t *canary_ptr = (uint64_t *)((char *)ptr + hdr->req_size);
    if (*canary_ptr != HEAP_CANARY_TAIL) {
        stats.corruptions_detected++;
        kpanic("heap: BUFFER OVERFLOW / TAIL CORRUPTION at %p (canary was 0x%016llx, expected 0x%016llx)",
               ptr, *canary_ptr, HEAP_CANARY_TAIL);
    }

    size_t req_size = hdr->req_size;
    uint32_t b_idx = hdr->bucket_idx;
    uint32_t pages = hdr->num_pages;

    /* Mark as free */
    hdr->magic = HEAP_MAGIC_FREE;

    if (stats.active_allocated_bytes >= req_size)
        stats.active_allocated_bytes -= req_size;
    stats.free_calls++;

    if (b_idx == BUCKET_LARGE) {
        uint64_t phys = vmm_virt_to_phys_hhdm(hdr);
        pmm_free_pages(phys, pages);
        if (stats.pages_used >= pages)
            stats.pages_used -= pages;
    } else if (b_idx < NUM_BUCKETS) {
        struct heap_bucket *b = &buckets[b_idx];
        struct heap_node *node = (struct heap_node *)hdr;
        node->next = b->free_list;
        b->free_list = node;
        b->frees++;
    } else {
        kpanic("heap: invalid bucket index %u in header %p", b_idx, (void *)hdr);
    }
}

void *krealloc(void *ptr, size_t new_size)
{
    if (!ptr)
        return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct heap_header *hdr = (struct heap_header *)((char *)ptr - sizeof(struct heap_header));
    if (hdr->magic != HEAP_MAGIC_ALLOC) {
        kpanic("heap: krealloc on invalid pointer %p", ptr);
    }

    size_t old_size = hdr->req_size;
    if (old_size >= new_size) {
        /* Already fits; update tail canary */
        hdr->req_size = new_size;
        uint64_t *canary_ptr = (uint64_t *)((char *)ptr + new_size);
        *canary_ptr = HEAP_CANARY_TAIL;
        return ptr;
    }

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr)
        return NULL;

    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);
    return new_ptr;
}

bool heap_verify(void)
{
    if (!initialized)
        return false;
    return (stats.corruptions_detected == 0 && stats.double_frees_prevented == 0);
}
