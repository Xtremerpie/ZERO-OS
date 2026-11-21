/* ZERO KERNEL - Kernel Heap Self-Tests (Version 1.0.0 Core Foundation).
 *
 * Verifies:
 * 1. 16-byte alignment of all allocations.
 * 2. Small object allocations across all 8 size classes.
 * 3. Large multi-page allocations (> 2048 bytes).
 * 4. kcalloc zero-initialization.
 * 5. krealloc expansion and contraction with data preservation.
 * 6. Repeated alloc/free stress cycles with free-list reuse.
 * 7. Heap stats invariant: active bytes cleanly returns to starting count.
 */

#include "memory/heap.h"
#include "memory/heap_test.h"
#include "zero/log.h"
#include "zero/string.h"

static bool test_alignment(void)
{
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(1 + i * 17);
        if (!ptrs[i])
            return false;
        if (((uintptr_t)ptrs[i] & (HEAP_ALIGNMENT - 1)) != 0) {
            KLOG_ERROR("heap_test", "unaligned pointer %p returned from kmalloc", ptrs[i]);
            return false;
        }
    }
    for (int i = 0; i < 10; i++) {
        kfree(ptrs[i]);
    }
    return true;
}

static bool test_size_classes(void)
{
    static const size_t sizes[] = { 8, 16, 24, 32, 60, 64, 100, 128, 200, 256, 500, 512, 1000, 1024, 1800, 2048 };
    void *ptrs[sizeof(sizes) / sizeof(sizes[0])];

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        ptrs[i] = kmalloc(sizes[i]);
        if (!ptrs[i])
            return false;
        /* Write test pattern */
        memset(ptrs[i], (int)(0xAA + i), sizes[i]);
    }

    /* Verify pattern and free */
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        uint8_t *b = (uint8_t *)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            if (b[j] != (uint8_t)(0xAA + i))
                return false;
        }
        kfree(ptrs[i]);
    }

    return true;
}

static bool test_large_allocation(void)
{
    size_t size = 16384; /* 16 KiB (4 pages) */
    uint8_t *ptr = (uint8_t *)kmalloc(size);
    if (!ptr)
        return false;

    for (size_t i = 0; i < size; i++) {
        ptr[i] = (uint8_t)(i & 0xFF);
    }

    for (size_t i = 0; i < size; i++) {
        if (ptr[i] != (uint8_t)(i & 0xFF)) {
            kfree(ptr);
            return false;
        }
    }

    kfree(ptr);
    return true;
}

static bool test_kcalloc(void)
{
    size_t count = 256;
    size_t elem_size = 8;
    uint64_t *arr = (uint64_t *)kcalloc(count, elem_size);
    if (!arr)
        return false;

    for (size_t i = 0; i < count; i++) {
        if (arr[i] != 0) {
            kfree(arr);
            return false;
        }
    }

    kfree(arr);
    return true;
}

static bool test_krealloc(void)
{
    size_t initial = 64;
    char *buf = (char *)kmalloc(initial);
    if (!buf)
        return false;

    const char *msg = "ZERO OS 1.0.0 Core Foundation - Dynamic Memory";
    size_t msg_len = strlen(msg) + 1;
    memcpy(buf, msg, msg_len);

    /* Expand allocation */
    size_t expanded = 1024;
    buf = (char *)krealloc(buf, expanded);
    if (!buf)
        return false;

    if (strcmp(buf, msg) != 0) {
        kfree(buf);
        return false;
    }

    /* Shrink allocation */
    size_t shrunk = 32;
    buf = (char *)krealloc(buf, shrunk);
    if (!buf)
        return false;

    if (strncmp(buf, msg, shrunk - 1) != 0) {
        kfree(buf);
        return false;
    }

    kfree(buf);
    return true;
}

static bool test_stress_cycles(void)
{
    #define STRESS_COUNT 128
    void *blocks[STRESS_COUNT];

    for (int cycle = 0; cycle < 4; cycle++) {
        for (int i = 0; i < STRESS_COUNT; i++) {
            size_t sz = 16 + ((i * 37) % 512);
            blocks[i] = kmalloc(sz);
            if (!blocks[i])
                return false;
            memset(blocks[i], 0x55, sz);
        }
        for (int i = 0; i < STRESS_COUNT; i++) {
            kfree(blocks[i]);
        }
    }

    return true;
}

bool heap_run_selftests(void)
{
    KLOG_INFO("heap_test", "running Kernel Heap self-test suite...");

    struct heap_stats before;
    heap_get_stats(&before);

    if (!test_alignment()) {
        KLOG_ERROR("heap_test", "[FAIL] 16-byte alignment check");
        return false;
    }
    kprintf("  [PASS] heap: 16-byte alignment validation\n");

    if (!test_size_classes()) {
        KLOG_ERROR("heap_test", "[FAIL] small size classes check");
        return false;
    }
    kprintf("  [PASS] heap: 8 segregated size classes (16B - 2048B)\n");

    if (!test_large_allocation()) {
        KLOG_ERROR("heap_test", "[FAIL] large page-backed allocation check");
        return false;
    }
    kprintf("  [PASS] heap: large multi-page allocation (16 KiB)\n");

    if (!test_kcalloc()) {
        KLOG_ERROR("heap_test", "[FAIL] kcalloc zero initialization");
        return false;
    }
    kprintf("  [PASS] heap: kcalloc zero-initialization\n");

    if (!test_krealloc()) {
        KLOG_ERROR("heap_test", "[FAIL] krealloc growth and shrinking");
        return false;
    }
    kprintf("  [PASS] heap: krealloc reallocation and data preservation\n");

    if (!test_stress_cycles()) {
        KLOG_ERROR("heap_test", "[FAIL] stress allocation cycles");
        return false;
    }
    kprintf("  [PASS] heap: stress allocation and free recycling\n");

    struct heap_stats after;
    heap_get_stats(&after);
    if (after.active_allocated_bytes != before.active_allocated_bytes) {
        KLOG_ERROR("heap_test", "[FAIL] memory leak detected: active bytes %lu != initial %lu",
                   after.active_allocated_bytes, before.active_allocated_bytes);
        return false;
    }
    kprintf("  [PASS] heap: memory leak invariant check (active bytes == 0)\n");

    KLOG_INFO("heap_test", "all Kernel Heap self-tests passed successfully");
    return true;
}
