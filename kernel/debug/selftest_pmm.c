/* ZERO KERNEL - PMM selftests (the 20 tests of the 1.0.0 directive) and benchmark.
 *
 * Tests on the REAL PMM use the bootloader's raw memory map as an independent oracle (they do not
 * trust the normalized map). Tests that need exact, deterministic layouts run on SYNTHETIC maps
 * with a private struct pmm whose bitmap lives in a page borrowed from the real PMM.
 * Randomness is a fixed-seed xorshift64*: every run is reproducible. */

#include <stdbool.h>
#include <stdint.h>
#include "limine.h"
#include "memory/pmm.h"
#include "memory/pmm_test.h"
#include "zero/checked.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"

extern char __kernel_start[], __kernel_end[];

#define CHECK(c)                                                                         \
    do {                                                                                 \
        if (!(c)) {                                                                      \
            kprintf("[FAIL] pmm:   check failed: %s (line %d)\n", #c, __LINE__);         \
            return false;                                                                \
        }                                                                                \
    } while (0)

#define MiB 0x100000ull

/* ---- independent oracle built from the raw Limine map ---- */

static uint64_t up(uint64_t v) { return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); }
static uint64_t down(uint64_t v) { return v & ~(PAGE_SIZE - 1); }

/* [a, a+len) is inside ONE bootloader USABLE entry, >= 1 MiB, and overlaps no non-usable entry,
 * the kernel image or the framebuffer. */
static bool oracle_ok(const struct boot_info *bi, uint64_t a, uint64_t len)
{
    const struct limine_memmap_response *r = bi->memmap;
    uint64_t e = a + len;
    bool inside = false;
    if (a < MiB)
        return false;
    for (uint64_t i = 0; i < r->entry_count; i++) {
        const struct limine_memmap_entry *m = r->entries[i];
        if (m->type == LIMINE_MEMMAP_USABLE) {
            if (a >= up(m->base) && e <= down(m->base + m->length))
                inside = true;
        } else if (a < up(m->base + m->length) && e > down(m->base)) {
            return false;
        }
    }
    if (bi->kernel_size && a < bi->kernel_phys_base + bi->kernel_size && e > bi->kernel_phys_base)
        return false;
    if (bi->fb_size && a < bi->fb_phys + bi->fb_size && e > bi->fb_phys)
        return false;
    return inside;
}

static void *phys_ptr(uint64_t a) { return (void *)(uintptr_t)(pmm_hhdm() + a); }

/* ---- fixtures ---- */

static uint64_t *store_get(uint64_t *phys)
{
    *phys = pmm_alloc_page();
    return *phys ? (uint64_t *)phys_ptr(*phys) : (uint64_t *)0;
}

#define STORE_WORDS 512 /* one page: tracks up to 32768 frames = 128 MiB of address span */

static bool synth_init(struct pmm *p, uint64_t *store, const struct mem_raw_entry *raw, uint64_t n)
{
    struct memmap m;
    if (memmap_build(&m, raw, n) != MEMMAP_OK)
        return false;
    pmm_instance_reset(p);
    return pmm_instance_init(p, &m, store, STORE_WORDS, 0, 0) == PMM_OK;
}

static uint64_t rng_state;
static void rng_seed(uint64_t s) { rng_state = s ? s : 0x9E3779B97F4A7C15ull; }
static uint64_t rng(void)
{
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 0x2545F4914F6CDD1Dull;
}

/* ---- the 20 tests ---- */

static bool t_init(const struct boot_info *bi)
{
    (void)bi;
    struct pmm *g = pmm_global();
    struct pmm_stats s;
    CHECK(pmm_is_initialized() && g);
    pmm_get_stats(&s);
    CHECK(s.total_memory > 0 && s.usable_memory > 0 && s.free_frames > 0);
    CHECK(s.free_frames <= s.allocatable_frames);
    CHECK(s.allocated_frames == s.allocatable_frames - s.free_frames);
    CHECK(pmm_instance_verify(g));
    return true;
}

static bool t_memmap_validation(const struct boot_info *bi)
{
    (void)bi;
    struct memmap m;
    /* unsorted input, unaligned edges, adjacent same-type merge */
    const struct mem_raw_entry a[] = {
        { 0x400000, 0x100000, MEM_USABLE },
        { 0x100800, 0x100000, MEM_USABLE },   /* -> [0x101000,0x200000) */
        { 0x1000, 0x9F000, MEM_USABLE },
        { 0x300000, 0x100000, MEM_USABLE },   /* merges with the 0x400000 entry */
        { 0x200800, 0x1000, MEM_RESERVED },   /* expands to [0x200000,0x202000) */
    };
    CHECK(memmap_build(&m, a, 5) == MEMMAP_OK);
    CHECK(m.count == 4);
    CHECK(m.r[0].base == 0x1000 && m.r[0].end == 0xA0000 && m.r[0].type == MEM_USABLE);
    CHECK(m.r[1].base == 0x101000 && m.r[1].end == 0x200000 && m.r[1].type == MEM_USABLE);
    CHECK(m.r[2].base == 0x200000 && m.r[2].end == 0x202000 && m.r[2].type == MEM_RESERVED);
    CHECK(m.r[3].base == 0x300000 && m.r[3].end == 0x500000 && m.r[3].type == MEM_USABLE);
    CHECK(m.bytes[MEM_USABLE] == 0x9F000 + 0xFF000 + 0x200000);
    CHECK(m.usable_end == 0x500000);
    CHECK(memmap_find(&m, 0x200000) && memmap_find(&m, 0x200000)->type == MEM_RESERVED);
    CHECK(memmap_find(&m, 0x250000) == (const struct mem_region *)0); /* hole */

    /* a page shared by two non-usable entries takes the higher-priority type */
    const struct mem_raw_entry b[] = {
        { 0x10000, 0x800, MEM_BOOTLOADER_RECLAIMABLE },
        { 0x10800, 0x800, MEM_KERNEL },
        { 0x20000, 0x800, MEM_RESERVED },
        { 0x20800, 0x800, MEM_ACPI_NVS },
        { 0x100000, 0x100000, MEM_USABLE },
    };
    CHECK(memmap_build(&m, b, 5) == MEMMAP_OK);
    CHECK(memmap_find(&m, 0x10000)->type == MEM_KERNEL);
    CHECK(memmap_find(&m, 0x20000)->type == MEM_ACPI_NVS);

    /* a usable partial page is never free: nothing left of a 0xC00-byte usable entry */
    const struct mem_raw_entry c[] = {
        { 0x5400, 0xC00, MEM_USABLE },
        { 0x100000, 0x1000, MEM_USABLE },
    };
    CHECK(memmap_build(&m, c, 2) == MEMMAP_OK);
    CHECK(m.count == 1 && m.r[0].base == 0x100000);

    /* the real map is sorted, aligned, non-overlapping, with no mergeable neighbours */
    CHECK(pmm_global() != (struct pmm *)0);
    const struct memmap *g = &pmm_global()->map;
    uint64_t sum = 0;
    for (uint32_t i = 0; i < g->count; i++) {
        CHECK(!(g->r[i].base & (PAGE_SIZE - 1)) && !(g->r[i].end & (PAGE_SIZE - 1)));
        CHECK(g->r[i].base < g->r[i].end);
        if (i) {
            CHECK(g->r[i - 1].end <= g->r[i].base);
            CHECK(!(g->r[i - 1].end == g->r[i].base && g->r[i - 1].type == g->r[i].type));
        }
        sum += g->r[i].end - g->r[i].base;
    }
    uint64_t tb = 0;
    for (int t = 0; t < MEM_TYPE_COUNT; t++)
        tb += g->bytes[t];
    CHECK(sum == tb);
    return true;
}

static bool t_bitmap_init(const struct boot_info *bi)
{
    uint64_t sp;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct memmap m;
    struct pmm_stats s;
    uint64_t addr = 0;

    /* usable 0x80000..0x140000 spans the 1 MiB line: only the 64 frames >= 1 MiB are allocatable */
    const struct mem_raw_entry r[] = {
        { 0x80000, 0x80000, MEM_USABLE },
        { 0x100000, 0x40000, MEM_USABLE },
    };
    CHECK(memmap_build(&m, r, 2) == MEMMAP_OK);
    pmm_instance_reset(&p);
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0, 0) == PMM_OK);
    pmm_instance_stats(&p, &s);
    CHECK(s.allocatable_frames == 64 && s.tracked_frames == 0x140);
    CHECK(pmm_instance_alloc(&p, 1, &addr) == PMM_OK && addr == 0x100000);
    CHECK(pmm_instance_frame_used(&p, 0x80000) && pmm_instance_frame_used(&p, 0xFF000));
    CHECK(pmm_instance_verify(&p));

    /* protected storage frames: marked used and not allocatable */
    pmm_instance_reset(&p);
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0x100000, 2) == PMM_OK);
    pmm_instance_stats(&p, &s);
    CHECK(s.allocatable_frames == 62);
    CHECK(pmm_instance_alloc(&p, 1, &addr) == PMM_OK && addr == 0x102000);
    CHECK(pmm_instance_free(&p, 0x100000, 1) == PMM_ERR_PROTECTED);
    /* bad protected ranges are refused */
    pmm_instance_reset(&p);
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0x80000, 1) == PMM_ERR_BAD_MAP); /* < 1 MiB */
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0x13F000, 2) == PMM_ERR_BAD_MAP); /* past region */
    CHECK(pmm_instance_init(&p, &m, store, 1, 0, 0) == PMM_ERR_NO_STORAGE);
    CHECK(pmm_free_page(sp) == PMM_OK);

    /* real map: independent recount from the bootloader entries */
    struct pmm *g = pmm_global();
    const struct limine_memmap_response *lr = bi->memmap;
    uint64_t frames = 0;
    for (uint64_t i = 0; i < lr->entry_count; i++) {
        const struct limine_memmap_entry *e = lr->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;
        uint64_t b = up(e->base), en = down(e->base + e->length);
        if (b < MiB)
            b = MiB;
        if (en > b)
            frames += (en - b) >> PAGE_SHIFT;
    }
    pmm_get_stats(&s);
    CHECK(frames - g->prot_frames == s.allocatable_frames);
    return true;
}

static bool t_single_alloc(const struct boot_info *bi)
{
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    uint64_t a = pmm_alloc_page(), b = pmm_alloc_page(), c = pmm_alloc_page();
    CHECK(a && b && c && a != b && b != c && a != c);
    CHECK(oracle_ok(bi, a, PAGE_SIZE) && oracle_ok(bi, b, PAGE_SIZE) && oracle_ok(bi, c, PAGE_SIZE));
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames - 3 && s1.allocated_frames == s0.allocated_frames + 3);
    CHECK(pmm_free_page(a) == PMM_OK && pmm_free_page(b) == PMM_OK && pmm_free_page(c) == PMM_OK);
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames);
    return true;
}

static bool t_multi_alloc(const struct boot_info *bi)
{
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    uint64_t a = pmm_alloc_pages(8);
    CHECK(a && oracle_ok(bi, a, 8 * PAGE_SIZE));
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames - 8);
    CHECK(pmm_alloc_pages(0) == 0); /* zero count is refused */
    CHECK(pmm_free_pages(a, 8) == PMM_OK);
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames);
    return true;
}

static bool t_contiguous(const struct boot_info *bi)
{
    (void)bi;
    uint64_t sp, a = 0;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct pmm_stats s;
    const struct mem_raw_entry r[] = { { 0x100000, 32 * PAGE_SIZE, MEM_USABLE } };
    CHECK(synth_init(&p, store, r, 1));
    for (int i = 0; i < 32; i++)
        CHECK(pmm_instance_alloc(&p, 1, &a) == PMM_OK && a == 0x100000 + (uint64_t)i * PAGE_SIZE);
    for (int i = 0; i < 32; i += 2) /* free frames 0,2,4,...: only isolated holes */
        CHECK(pmm_instance_free(&p, 0x100000 + (uint64_t)i * PAGE_SIZE, 1) == PMM_OK);
    CHECK(pmm_instance_alloc(&p, 2, &a) == PMM_ERR_NO_MEMORY && a == 0);
    pmm_instance_stats(&p, &s);
    CHECK(s.free_frames == 16 && s.alloc_failures == 1);
    CHECK(pmm_instance_free(&p, 0x101000, 1) == PMM_OK); /* frames 0,1,2 now free */
    CHECK(pmm_instance_alloc(&p, 3, &a) == PMM_OK && a == 0x100000);
    CHECK(pmm_instance_alloc(&p, 2, &a) == PMM_ERR_NO_MEMORY);
    CHECK(pmm_instance_verify(&p));
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_alignment(const struct boot_info *bi)
{
    (void)bi;
    for (int i = 0; i < 16; i++) {
        uint64_t n = 1 + (uint64_t)i % 5;
        uint64_t a = pmm_alloc_pages(n);
        CHECK(a && !(a & (PAGE_SIZE - 1)));
        CHECK(pmm_free_pages(a | 0x10, n) == PMM_ERR_UNALIGNED);
        CHECK(pmm_free_pages(a, n) == PMM_OK);
    }
    return true;
}

static bool t_exhaustion(const struct boot_info *bi)
{
    (void)bi;
    uint64_t sp, a = 0, n = 0, prev = 0;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct pmm_stats s;
    const struct mem_raw_entry r[] = { { 0x100000, 1000 * PAGE_SIZE, MEM_USABLE } };
    CHECK(synth_init(&p, store, r, 1));
    while (pmm_instance_alloc(&p, 1, &a) == PMM_OK) {
        CHECK(a > prev || n == 0);
        prev = a;
        n++;
        CHECK(n <= 1000);
    }
    CHECK(n == 1000 && a == 0);
    pmm_instance_stats(&p, &s);
    CHECK(s.free_frames == 0 && s.allocated_frames == 1000 && s.peak_allocated_frames == 1000);
    CHECK(pmm_instance_alloc(&p, 1, &a) == PMM_ERR_NO_MEMORY);
    CHECK(pmm_instance_alloc(&p, 5, &a) == PMM_ERR_NO_MEMORY);
    pmm_instance_stats(&p, &s);
    CHECK(s.alloc_failures == 3); /* the loop's terminating failure + the two above */
    CHECK(pmm_instance_verify(&p));
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_free(const struct boot_info *bi)
{
    (void)bi;
    uint64_t sp, a[5], x = 0;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct pmm_stats s0, s1;
    const struct mem_raw_entry r[] = { { 0x100000, 64 * PAGE_SIZE, MEM_USABLE } };
    CHECK(synth_init(&p, store, r, 1));
    for (int i = 0; i < 5; i++)
        CHECK(pmm_instance_alloc(&p, 1, &a[i]) == PMM_OK);
    pmm_instance_stats(&p, &s0);
    CHECK(pmm_instance_free(&p, a[2], 1) == PMM_OK);
    pmm_instance_stats(&p, &s1);
    CHECK(s1.free_frames == s0.free_frames + 1);
    CHECK(pmm_instance_alloc(&p, 1, &x) == PMM_OK && x == a[2]); /* lowest free frame is reused */
    CHECK(pmm_instance_free(&p, a[0], 5) == PMM_OK);              /* multi-frame free of a run */
    pmm_instance_stats(&p, &s1);
    CHECK(s1.free_frames == 64);
    CHECK(pmm_instance_verify(&p));
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_double_free(const struct boot_info *bi)
{
    (void)bi;
    uint64_t sp, a = 0;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct pmm_stats s;
    const struct mem_raw_entry r[] = { { 0x100000, 64 * PAGE_SIZE, MEM_USABLE } };
    CHECK(synth_init(&p, store, r, 1));
    CHECK(pmm_instance_alloc(&p, 1, &a) == PMM_OK);
    CHECK(pmm_instance_free(&p, a, 1) == PMM_OK);
    CHECK(pmm_instance_free(&p, a, 1) == PMM_ERR_NOT_ALLOCATED);
    pmm_instance_stats(&p, &s);
    CHECK(s.free_frames == 64 && s.double_frees == 1 && s.invalid_frees == 1);

    /* atomicity: a run with one already-free frame frees NOTHING */
    CHECK(pmm_instance_alloc(&p, 4, &a) == PMM_OK);
    CHECK(pmm_instance_free(&p, a + PAGE_SIZE, 1) == PMM_OK);
    CHECK(pmm_instance_free(&p, a, 4) == PMM_ERR_NOT_ALLOCATED);
    CHECK(pmm_instance_frame_used(&p, a) && !pmm_instance_frame_used(&p, a + PAGE_SIZE) &&
          pmm_instance_frame_used(&p, a + 2 * PAGE_SIZE) && pmm_instance_frame_used(&p, a + 3 * PAGE_SIZE));
    pmm_instance_stats(&p, &s);
    CHECK(s.free_frames == 61);
    CHECK(pmm_instance_verify(&p));
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_invalid_free(const struct boot_info *bi)
{
    (void)bi;
    uint64_t sp, a = 0;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct pmm_stats s;
    const struct mem_raw_entry r[] = {
        { 0x100000, 16 * PAGE_SIZE, MEM_USABLE },
        { 0x200000, 16 * PAGE_SIZE, MEM_USABLE },
    };
    CHECK(synth_init(&p, store, r, 2));
    CHECK(pmm_instance_alloc(&p, 16, &a) == PMM_OK && a == 0x100000);
    CHECK(pmm_instance_free(&p, 0x100001, 1) == PMM_ERR_UNALIGNED);
    CHECK(pmm_instance_free(&p, 0x100000, 0) == PMM_ERR_COUNT);
    CHECK(pmm_instance_free(&p, 0, 1) == PMM_ERR_RANGE);                  /* hole */
    CHECK(pmm_instance_free(&p, 0x150000, 1) == PMM_ERR_RANGE);           /* hole between regions */
    CHECK(pmm_instance_free(&p, 0x300000, 1) == PMM_ERR_RANGE);           /* beyond everything */
    CHECK(pmm_instance_free(&p, 0xFFFFFFFFFFFFF000ull, 1) == PMM_ERR_RANGE); /* end wraps */
    CHECK(pmm_instance_free(&p, 0x100000, 1ull << 60) == PMM_ERR_RANGE);  /* count*4096 wraps */
    CHECK(pmm_instance_free(&p, 0x100000, 1ull << 40) == PMM_ERR_RANGE);  /* beyond 2^52 */
    CHECK(pmm_instance_free(&p, 0x100000 + 15 * PAGE_SIZE, 2) == PMM_ERR_RANGE); /* runs into a hole */
    CHECK(pmm_instance_free(&p, 0x200000, 1) == PMM_ERR_NOT_ALLOCATED);   /* never allocated */
    pmm_instance_stats(&p, &s);
    CHECK(s.invalid_frees == 10 && s.free_frames == 16);
    CHECK(pmm_instance_frame_used(&p, 0x100000 + 15 * PAGE_SIZE)); /* untouched by the rejects */
    CHECK(pmm_instance_verify(&p));
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_reserved_protection(const struct boot_info *bi)
{
    (void)bi;
    struct pmm *g = pmm_global();
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    unsigned tested = 0;
    for (uint32_t i = 0; i < g->map.count; i++) {
        const struct mem_region *r = &g->map.r[i];
        if (r->type == MEM_USABLE)
            continue;
        CHECK(pmm_free_page(r->base) == PMM_ERR_PROTECTED);
        CHECK(pmm_free_page(r->end - PAGE_SIZE) == PMM_ERR_PROTECTED);
        tested++;
    }
    CHECK(tested > 0);
    for (uint32_t i = 0; i < g->map.count; i++) { /* usable memory below 1 MiB is policy-protected */
        const struct mem_region *r = &g->map.r[i];
        if (r->type == MEM_USABLE && r->base < PMM_LOW_LIMIT)
            CHECK(pmm_free_page(r->base) == PMM_ERR_PROTECTED);
    }
    CHECK(g->prot_frames > 0 && pmm_free_page(g->prot_first << PAGE_SHIFT) == PMM_ERR_PROTECTED);
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames && s1.invalid_frees > s0.invalid_frees);
    return true;
}

static bool t_kernel_region(const struct boot_info *bi)
{
    CHECK(bi->kernel_size > 0 && !(bi->kernel_phys_base & (PAGE_SIZE - 1)));
    struct pmm *g = pmm_global();
    uint64_t kb = bi->kernel_phys_base, ke = up(kb + bi->kernel_size);
    CHECK((uint64_t)(uintptr_t)(__kernel_end - __kernel_start) == bi->kernel_size);
    for (uint64_t a = kb; a < ke; a += PAGE_SIZE) {
        const struct mem_region *r = memmap_find(&g->map, a);
        CHECK(r && r->type == MEM_KERNEL);
        CHECK(pmm_free_page(a) == PMM_ERR_PROTECTED);
    }
    uint64_t a = pmm_alloc_pages(256);
    CHECK(a && !(a < ke && a + 256 * PAGE_SIZE > kb));
    CHECK(pmm_free_pages(a, 256) == PMM_OK);
    return true;
}

static bool t_bitmap_boundary(const struct boot_info *bi)
{
    (void)bi;
    static const uint64_t sizes[] = { 1, 63, 64, 65, 127, 128, 129 };
    uint64_t sp, a = 0;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    for (unsigned k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        uint64_t n = sizes[k];
        const struct mem_raw_entry r[] = { { 0x100000, n * PAGE_SIZE, MEM_USABLE } };
        CHECK(synth_init(&p, store, r, 1));
        uint64_t end = 0x100000 + n * PAGE_SIZE, cnt = 0;
        while (pmm_instance_alloc(&p, 1, &a) == PMM_OK) {
            CHECK(a >= 0x100000 && a < end);
            cnt++;
            CHECK(cnt <= n);
        }
        CHECK(cnt == n);
        CHECK(pmm_instance_verify(&p)); /* padding bits intact */
        CHECK(pmm_instance_free(&p, end, 1) == PMM_ERR_RANGE);          /* first frame past the end */
        CHECK(pmm_instance_free(&p, end - PAGE_SIZE, 1) == PMM_OK);     /* last frame */
        CHECK(pmm_instance_alloc(&p, 2, &a) == PMM_ERR_NO_MEMORY);      /* cannot spill past the end */
        CHECK(pmm_instance_alloc(&p, 1, &a) == PMM_OK && a == end - PAGE_SIZE);
    }
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_memmap_overflow(const struct boot_info *bi)
{
    (void)bi;
    struct memmap m;
    struct mem_raw_entry big[MEMMAP_MAX_RAW + 1];
    const struct mem_raw_entry ok = { 0x100000, 0x100000, MEM_USABLE };

    const struct mem_raw_entry wrap1[] = { ok, { 0xFFFFFFFFFFFFF000ull, 0x2000, MEM_RESERVED } };
    const struct mem_raw_entry wrap2[] = { ok, { 0x1000, ~0ull, MEM_RESERVED } };
    const struct mem_raw_entry far1[] = { ok, { 1ull << 52, 0x1000, MEM_RESERVED } };
    const struct mem_raw_entry badt[] = { ok, { 0x400000, 0x1000, (enum mem_type)99 } };
    const struct mem_raw_entry ovl[] = { ok, { 0x180000, 0x100000, MEM_RESERVED } };
    const struct mem_raw_entry dup[] = { ok, ok };
    const struct mem_raw_entry nous[] = { { 0x100000, 0x100000, MEM_RESERVED } };
    const struct mem_raw_entry tiny[] = { { 0x100000, 0x800, MEM_USABLE } }; /* < 1 page: nothing usable */

    CHECK(memmap_build(&m, wrap1, 2) == MEMMAP_ERR_OVERFLOW && m.count == 0);
    CHECK(memmap_build(&m, wrap2, 2) == MEMMAP_ERR_OVERFLOW);
    CHECK(memmap_build(&m, far1, 2) == MEMMAP_ERR_RANGE);
    CHECK(memmap_build(&m, badt, 2) == MEMMAP_ERR_RANGE);
    CHECK(memmap_build(&m, ovl, 2) == MEMMAP_ERR_OVERLAP && m.count == 0);
    CHECK(memmap_build(&m, dup, 2) == MEMMAP_ERR_OVERLAP);
    CHECK(memmap_build(&m, nous, 1) == MEMMAP_ERR_NO_USABLE);
    CHECK(memmap_build(&m, tiny, 1) == MEMMAP_ERR_NO_USABLE);
    CHECK(memmap_build(&m, (const struct mem_raw_entry *)0, 1) == MEMMAP_ERR_NULL);
    CHECK(memmap_build((struct memmap *)0, &ok, 1) == MEMMAP_ERR_NULL);
    CHECK(memmap_build(&m, &ok, 0) == MEMMAP_ERR_COUNT);
    for (unsigned i = 0; i < MEMMAP_MAX_RAW + 1; i++) {
        big[i].base = 0x100000 + (uint64_t)i * 0x2000;
        big[i].length = 0x1000;
        big[i].type = (i & 1) ? MEM_RESERVED : MEM_USABLE;
    }
    CHECK(memmap_build(&m, big, MEMMAP_MAX_RAW + 1) == MEMMAP_ERR_COUNT);
    CHECK(memmap_build(&m, big, MEMMAP_MAX_RAW) == MEMMAP_OK); /* exactly at the limit is fine */

    /* PMM-level hostile input */
    uint64_t sp;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    pmm_instance_reset(&p);
    memset(&m, 0, sizeof m);
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0, 0) == PMM_ERR_BAD_MAP); /* empty map */
    CHECK(pmm_instance_init(&p, (const struct memmap *)0, store, STORE_WORDS, 0, 0) == PMM_ERR_BAD_MAP);
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

static bool t_cycles(const struct boot_info *bi)
{
    (void)bi;
    struct pmm *g = pmm_global();
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    for (unsigned i = 0; i < 20000; i++) {
        uint64_t n = (i % 7 == 0) ? 1 + (i % 16) : 1;
        uint64_t a = pmm_alloc_pages(n);
        CHECK(a != 0);
        CHECK(pmm_free_pages(a, n) == PMM_OK);
    }
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames);
    CHECK(s1.alloc_calls == s0.alloc_calls + 20000 && s1.free_calls == s0.free_calls + 20000);
    CHECK(pmm_instance_verify(g));
    return true;
}

static bool t_stats(const struct boot_info *bi)
{
    (void)bi;
    struct pmm_stats s;
    uint64_t sp, a = 0, x;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    const struct mem_raw_entry r[] = {
        { 0x100000, 100 * PAGE_SIZE, MEM_USABLE },
        { 0x200000, 8 * PAGE_SIZE, MEM_RESERVED },
    };
    CHECK(synth_init(&p, store, r, 2));
    for (int i = 0; i < 10; i++)
        CHECK(pmm_instance_alloc(&p, 1, &a) == PMM_OK);
    CHECK(pmm_instance_alloc(&p, 200, &x) == PMM_ERR_NO_MEMORY);
    pmm_instance_stats(&p, &s);
    CHECK(s.total_memory == 108 * PAGE_SIZE && s.usable_memory == 100 * PAGE_SIZE);
    CHECK(s.reserved_memory == 8 * PAGE_SIZE && s.total_frames == 108);
    CHECK(s.allocatable_frames == 100 && s.free_frames == 90 && s.allocated_frames == 10);
    CHECK(s.free_memory == 90 * PAGE_SIZE && s.allocated_memory == 10 * PAGE_SIZE);
    CHECK(s.alloc_calls == 11 && s.alloc_failures == 1 && s.peak_allocated_frames == 10);
    for (int i = 0; i < 10; i++)
        CHECK(pmm_instance_free(&p, 0x100000 + (uint64_t)i * PAGE_SIZE, 1) == PMM_OK);
    pmm_instance_stats(&p, &s);
    CHECK(s.free_calls == 10 && s.peak_allocated_frames == 10 && s.allocated_frames == 0);
    CHECK(pmm_instance_verify(&p));
    CHECK(pmm_free_page(sp) == PMM_OK);

    /* real PMM invariants */
    pmm_get_stats(&s);
    uint64_t t = 0;
    for (int i = 0; i < MEM_TYPE_COUNT; i++)
        t += pmm_global()->map.bytes[i];
    CHECK(s.total_memory == t && s.reserved_memory == s.total_memory - s.usable_memory);
    CHECK(s.free_frames + s.allocated_frames == s.allocatable_frames);
    CHECK(s.allocatable_memory <= s.usable_memory && s.peak_allocated_frames >= s.allocated_frames);
    CHECK(s.free_memory == s.free_frames * PAGE_SIZE);
    CHECK(pmm_instance_verify(pmm_global()));
    return true;
}

static bool t_repeated_init(const struct boot_info *bi)
{
    (void)bi;
    struct pmm *g = pmm_global();
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    CHECK(pmm_init(&g->map, pmm_hhdm()) == PMM_ERR_ALREADY_INITIALIZED);
    CHECK(pmm_init(&g->map, pmm_hhdm()) == PMM_ERR_ALREADY_INITIALIZED);
    pmm_get_stats(&s1);
    CHECK(memcmp(&s0, &s1, sizeof s0) == 0);
    CHECK(pmm_instance_verify(g));

    uint64_t sp;
    uint64_t *store = store_get(&sp);
    CHECK(store);
    struct pmm p;
    struct memmap m;
    const struct mem_raw_entry r[] = { { 0x100000, 16 * PAGE_SIZE, MEM_USABLE } };
    CHECK(memmap_build(&m, r, 1) == MEMMAP_OK);
    pmm_instance_reset(&p);
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0, 0) == PMM_OK);
    uint64_t a;
    CHECK(pmm_instance_alloc(&p, 3, &a) == PMM_OK);
    CHECK(pmm_instance_init(&p, &m, store, STORE_WORDS, 0, 0) == PMM_ERR_ALREADY_INITIALIZED);
    CHECK(pmm_instance_frame_used(&p, a)); /* the refused re-init did not wipe state */
    CHECK(pmm_free_page(sp) == PMM_OK);
    return true;
}

#define RAND_TABLE 1024
struct live { uint64_t addr, count; };
#define STAMP(a) ((a) ^ 0xA5A5C3C35A5A3C3Cull)

static bool t_random(const struct boot_info *bi)
{
    static const uint64_t seeds[] = { 1, 0xDEADBEEFull, 0x123456789ABCDEFull };
    struct pmm *g = pmm_global();
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0); /* before the table is allocated: the end state must equal this */
    uint64_t tp = pmm_alloc_pages(RAND_TABLE * sizeof(struct live) / PAGE_SIZE);
    CHECK(tp);
    struct live *tab = (struct live *)phys_ptr(tp);
    for (unsigned sd = 0; sd < 3; sd++) {
        rng_seed(seeds[sd]);
        unsigned live = 0;
        for (unsigned op = 0; op < 4000; op++) {
            bool do_alloc = live == 0 || (live < RAND_TABLE && rng() % 3 != 0);
            if (do_alloc) {
                uint64_t n = (rng() % 16 == 0) ? 32 : 1 + rng() % 8;
                uint64_t a = pmm_alloc_pages(n);
                if (!a)
                    continue;
                CHECK(oracle_ok(bi, a, n * PAGE_SIZE));
                for (uint64_t i = 0; i < n; i++) {
                    uint64_t *w = (uint64_t *)phys_ptr(a + i * PAGE_SIZE);
                    CHECK(*w != STAMP(a + i * PAGE_SIZE)); /* already stamped => handed out twice */
                    *w = STAMP(a + i * PAGE_SIZE);
                }
                tab[live].addr = a;
                tab[live].count = n;
                live++;
            } else {
                unsigned k = (unsigned)(rng() % live);
                struct live e = tab[k];
                tab[k] = tab[--live];
                for (uint64_t i = 0; i < e.count; i++) {
                    uint64_t *w = (uint64_t *)phys_ptr(e.addr + i * PAGE_SIZE);
                    CHECK(*w == STAMP(e.addr + i * PAGE_SIZE)); /* nobody else overwrote it */
                    *w = 0;
                }
                CHECK(pmm_free_pages(e.addr, e.count) == PMM_OK);
            }
        }
        while (live) {
            struct live e = tab[--live];
            for (uint64_t i = 0; i < e.count; i++)
                *(uint64_t *)phys_ptr(e.addr + i * PAGE_SIZE) = 0;
            CHECK(pmm_free_pages(e.addr, e.count) == PMM_OK);
        }
        CHECK(pmm_instance_verify(g));
    }
    CHECK(pmm_free_pages(tp, RAND_TABLE * sizeof(struct live) / PAGE_SIZE) == PMM_OK);
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames);
    return true;
}

static bool t_exhaustion_recovery(const struct boot_info *bi)
{
    struct pmm *g = pmm_global();
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    uint64_t head = 0, n = 0;
    for (;;) {
        uint64_t a = pmm_alloc_page();
        if (!a)
            break;
        CHECK(oracle_ok(bi, a, PAGE_SIZE)); /* never kernel, framebuffer, reserved, <1 MiB ... */
        uint64_t *w = (uint64_t *)phys_ptr(a);
        CHECK(w[0] != STAMP(a));
        w[0] = STAMP(a);
        w[1] = head;
        head = a;
        n++;
    }
    CHECK(n == s0.free_frames);
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == 0 && s1.allocated_frames == s1.allocatable_frames);
    CHECK(pmm_alloc_page() == 0 && pmm_alloc_pages(2) == 0);
    CHECK(pmm_instance_verify(g));
    while (head) {
        uint64_t *w = (uint64_t *)phys_ptr(head);
        CHECK(w[0] == STAMP(head));
        uint64_t next = w[1];
        w[0] = 0;
        CHECK(pmm_free_page(head) == PMM_OK);
        head = next;
    }
    pmm_get_stats(&s1);
    CHECK(s1.free_frames == s0.free_frames && s1.peak_allocated_frames >= s0.allocatable_frames);
    CHECK(pmm_instance_verify(g));
    uint64_t a = pmm_alloc_pages(16); /* usable again after exhaustion */
    CHECK(a && pmm_free_pages(a, 16) == PMM_OK);
    return true;
}

struct test {
    const char *name;
    bool (*fn)(const struct boot_info *);
};

static const struct test tests[] = {
    { "init", t_init },
    { "memmap_validation", t_memmap_validation },
    { "bitmap_init", t_bitmap_init },
    { "single_alloc", t_single_alloc },
    { "multi_alloc", t_multi_alloc },
    { "contiguous_alloc", t_contiguous },
    { "alignment", t_alignment },
    { "exhaustion", t_exhaustion },
    { "free", t_free },
    { "double_free", t_double_free },
    { "invalid_free", t_invalid_free },
    { "reserved_protection", t_reserved_protection },
    { "kernel_region_protection", t_kernel_region },
    { "bitmap_boundary", t_bitmap_boundary },
    { "memmap_overflow", t_memmap_overflow },
    { "alloc_free_cycles", t_cycles },
    { "stats_consistency", t_stats },
    { "repeated_init", t_repeated_init },
    { "randomized_sequences", t_random },
    { "exhaustion_recovery", t_exhaustion_recovery },
};

unsigned pmm_selftest_all(const struct boot_info *bi)
{
    unsigned failed = 0, n = sizeof tests / sizeof tests[0];
    if (!pmm_is_initialized() || !bi || !bi->memmap) {
        kprintf("[FAIL] pmm: selftest prerequisites missing\n");
        return n;
    }
    for (unsigned i = 0; i < n; i++) {
        if (tests[i].fn(bi))
            kprintf("[PASS] pmm: %s\n", tests[i].name);
        else {
            kprintf("[FAIL] pmm: %s\n", tests[i].name);
            failed++;
        }
    }
    KLOG_INFO("pmm", "selftest: %u/%u passed", n - failed, n);
    return failed;
}

/* ---- always-on boot check ---- */

bool pmm_boot_check(void)
{
    struct pmm_stats s0, s1;
    pmm_get_stats(&s0);
    uint64_t a = pmm_alloc_page();
    if (!a || (a & (PAGE_SIZE - 1)))
        return false;
    uint64_t *w = (uint64_t *)phys_ptr(a);
    w[0] = 0x5A5A5A5A5A5A5A5Aull; /* the page is really ours and writable through the HHDM */
    if (w[0] != 0x5A5A5A5A5A5A5A5Aull)
        return false;
    pmm_get_stats(&s1);
    if (s1.free_frames != s0.free_frames - 1)
        return false;
    if (pmm_free_page(a) != PMM_OK || pmm_free_page(a) != PMM_ERR_NOT_ALLOCATED)
        return false;
    pmm_get_stats(&s1);
    return s1.free_frames == s0.free_frames && pmm_instance_verify(pmm_global());
}

/* ---- benchmark ---- */

static void bench_line(const char *what, uint64_t cycles, uint64_t ops)
{
    kprintf("[BENCH] pmm: %-28s %10lu cycles total, %8lu cycles/op (%lu ops)\n", what, cycles,
            ops ? cycles / ops : 0, ops);
}

void pmm_selftest_bench(const struct boot_info *bi)
{
    (void)bi;
    struct pmm *g = pmm_global();
    struct pmm_stats s;
    pmm_get_stats(&s);
    kprintf("[BENCH] pmm: units are TSC cycles; under QEMU TCG the TSC is virtual (not real CPU cycles)\n");
    kprintf("[BENCH] pmm: guest memory %lu KiB total, %lu frames tracked, bitmap %lu bytes (%lu KiB)\n",
            s.total_memory / 1024, s.tracked_frames, s.bitmap_bytes, s.bitmap_bytes / 1024);

    /* init cost: rebuild the normalized map and a second bitmap in borrowed frames */
    uint64_t words = g->words, pages = (words * 8 + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t sp = pmm_alloc_pages(pages);
    uint64_t t0, t1;
    if (sp) {
        static struct pmm scratch;
        t0 = rdtsc();
        pmm_instance_reset(&scratch);
        enum pmm_status st = pmm_instance_init(&scratch, &g->map, (uint64_t *)phys_ptr(sp), words, 0, 0);
        t1 = rdtsc();
        bench_line(st == PMM_OK ? "pmm init (bitmap build)" : "pmm init FAILED", t1 - t0, 1);
        pmm_free_pages(sp, pages);
    }

    enum { N = 20000 };
    uint64_t tp = pmm_alloc_pages(N * 8 / PAGE_SIZE + 1);
    if (!tp) {
        kprintf("[BENCH] pmm: cannot allocate scratch table; benchmark skipped\n");
        return;
    }
    uint64_t *tab = (uint64_t *)phys_ptr(tp);
    uint64_t n = 0;

    t0 = rdtsc();
    for (; n < N; n++)
        if (!(tab[n] = pmm_alloc_page()))
            break;
    t1 = rdtsc();
    bench_line("alloc 1 page", t1 - t0, n);
    uint64_t got = n;
    t0 = rdtsc();
    for (uint64_t i = 0; i < got; i++)
        pmm_free_page(tab[i]);
    t1 = rdtsc();
    bench_line("free 1 page", t1 - t0, got);

    uint64_t cnt = 0;
    t0 = rdtsc();
    for (; cnt < 2000; cnt++)
        if (!(tab[cnt] = pmm_alloc_pages(16)))
            break;
    t1 = rdtsc();
    bench_line("alloc 16 contiguous pages", t1 - t0, cnt);
    uint64_t got16 = cnt;
    t0 = rdtsc();
    for (uint64_t i = 0; i < got16; i++)
        pmm_free_pages(tab[i], 16);
    t1 = rdtsc();
    bench_line("free 16 contiguous pages", t1 - t0, got16);

    /* fragmentation: allocate 8192 single pages, free every other one */
    uint64_t m = 0;
    for (; m < 8192; m++)
        if (!(tab[m] = pmm_alloc_page()))
            break;
    for (uint64_t i = 0; i < m; i += 2)
        pmm_free_page(tab[i]);
    uint64_t runs, longest;
    t0 = rdtsc();
    pmm_instance_fragmentation(g, &runs, &longest);
    t1 = rdtsc();
    kprintf("[BENCH] pmm: fragmented (every 2nd of %lu pages freed): %lu free runs, longest %lu frames; "
            "scan %lu cycles\n", m, runs, longest, t1 - t0);
    uint64_t big = pmm_alloc_pages(64); /* first-fit must skip the 1-frame holes */
    kprintf("[BENCH] pmm: 64-frame request in fragmented memory: %s\n", big ? "satisfied" : "FAILED");
    if (big)
        pmm_free_pages(big, 64);
    for (uint64_t i = 1; i < m; i += 2)
        pmm_free_page(tab[i]);
    pmm_free_pages(tp, N * 8 / PAGE_SIZE + 1);

    pmm_instance_fragmentation(g, &runs, &longest);
    pmm_get_stats(&s);
    kprintf("[BENCH] pmm: after benchmark: %lu free runs, longest %lu frames; free %lu/%lu frames; "
            "peak allocated %lu frames (%lu%% of allocatable)\n", runs, longest, s.free_frames,
            s.allocatable_frames, s.peak_allocated_frames,
            s.allocatable_frames ? s.peak_allocated_frames * 100 / s.allocatable_frames : 0);
}
