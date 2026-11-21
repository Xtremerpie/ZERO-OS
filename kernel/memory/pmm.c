/* ZERO KERNEL - bitmap physical memory manager. See pmm.h and docs/PMM_DESIGN.md. */

#include "memory/pmm.h"
#include "zero/checked.h"
#include "zero/string.h"

#define ALL_ONES (~0ull)

/* ---- bitmap primitives (callers guarantee idx < p->frames unless noted) ---- */

static inline bool bm_test(const struct pmm *p, uint64_t i)
{
    return (p->bitmap[i >> 6] >> (i & 63)) & 1;
}
static inline void bm_set(struct pmm *p, uint64_t i)
{
    p->bitmap[i >> 6] |= 1ull << (i & 63);
}
static inline void bm_clear(struct pmm *p, uint64_t i)
{
    p->bitmap[i >> 6] &= ~(1ull << (i & 63));
}

/* First clear bit at index >= from, or p->frames if none. */
static uint64_t next_clear(const struct pmm *p, uint64_t from)
{
    if (from >= p->frames)
        return p->frames;
    uint64_t w = from >> 6;
    uint64_t bits = ~p->bitmap[w] & (ALL_ONES << (from & 63));
    for (;;) {
        if (bits) {
            uint64_t r = (w << 6) + (uint64_t)__builtin_ctzll(bits);
            return r < p->frames ? r : p->frames; /* padding bits are 1, so r < frames anyway */
        }
        if (++w >= p->words)
            return p->frames;
        bits = ~p->bitmap[w];
    }
}

/* First set bit at index >= from, or p->frames if none. */
static uint64_t next_set(const struct pmm *p, uint64_t from)
{
    if (from >= p->frames)
        return p->frames;
    uint64_t w = from >> 6;
    uint64_t bits = p->bitmap[w] & (ALL_ONES << (from & 63));
    for (;;) {
        if (bits) {
            uint64_t r = (w << 6) + (uint64_t)__builtin_ctzll(bits);
            return r < p->frames ? r : p->frames;
        }
        if (++w >= p->words)
            return p->frames;
        bits = p->bitmap[w];
    }
}

/* First set bit in [from, limit), or `limit` if there is none (limit <= p->frames). Bounded so that
 * checking whether a free run is long enough never scans past the length that was asked for. */
static uint64_t next_set_lim(const struct pmm *p, uint64_t from, uint64_t limit)
{
    if (from >= limit)
        return limit;
    uint64_t w = from >> 6;
    uint64_t bits = p->bitmap[w] & (ALL_ONES << (from & 63));
    for (;;) {
        if (bits) {
            uint64_t r = (w << 6) + (uint64_t)__builtin_ctzll(bits);
            return r < limit ? r : limit;
        }
        if (((++w) << 6) >= limit)
            return limit;
        bits = p->bitmap[w];
    }
}

static void note_peak(struct pmm *p)
{
    uint64_t used = p->allocatable - p->free_frames;
    if (used > p->peak_used)
        p->peak_used = used;
}

/* ---- instance API ---- */

void pmm_instance_reset(struct pmm *p)
{
    memset(p, 0, sizeof *p);
}

enum pmm_status pmm_instance_init(struct pmm *p, const struct memmap *m, uint64_t *storage,
                                  uint64_t storage_words, uint64_t prot_base, uint64_t prot_frames)
{
    if (!p || !m || !storage)
        return PMM_ERR_BAD_MAP;
    if (p->initialized)
        return PMM_ERR_ALREADY_INITIALIZED;
    if (m->count == 0 || m->count > MEMMAP_MAX_REGIONS || m->usable_end == 0 ||
        m->usable_end > MEMMAP_MAX_ADDR || (m->usable_end & (PAGE_SIZE - 1)))
        return PMM_ERR_BAD_MAP;

    uint64_t frames = m->usable_end >> PAGE_SHIFT;
    uint64_t words = (frames + 63) >> 6;
    if (storage_words < words)
        return PMM_ERR_NO_STORAGE;

    /* The protected range (bitmap storage) must lie inside one USABLE region at/above 1 MiB. */
    uint64_t prot_first = 0;
    if (prot_frames) {
        uint64_t prot_bytes, prot_end;
        if ((prot_base & (PAGE_SIZE - 1)) || prot_base < PMM_LOW_LIMIT ||
            chk_mul_u64(prot_frames, PAGE_SIZE, &prot_bytes) ||
            chk_add_u64(prot_base, prot_bytes, &prot_end))
            return PMM_ERR_BAD_MAP;
        const struct mem_region *r = memmap_find(m, prot_base);
        if (!r || r->type != MEM_USABLE || prot_end > r->end)
            return PMM_ERR_BAD_MAP;
        prot_first = prot_base >> PAGE_SHIFT;
    }

    pmm_instance_reset(p);
    memcpy(&p->map, m, sizeof *m);
    p->bitmap = storage;
    p->words = words;
    p->frames = frames;
    for (uint64_t w = 0; w < words; w++)
        storage[w] = ALL_ONES; /* everything used; padding bits stay 1 forever */

    for (uint32_t i = 0; i < m->count; i++) {
        if (m->r[i].type != MEM_USABLE)
            continue;
        uint64_t b = m->r[i].base < PMM_LOW_LIMIT ? PMM_LOW_LIMIT : m->r[i].base;
        for (uint64_t f = b >> PAGE_SHIFT; f < (m->r[i].end >> PAGE_SHIFT); f++)
            bm_clear(p, f);
    }
    for (uint64_t f = 0; f < prot_frames; f++)
        bm_set(p, prot_first + f);
    p->prot_first = prot_first;
    p->prot_frames = prot_frames;

    uint64_t freec = 0;
    for (uint64_t w = 0; w < words; w++)
        freec += 64 - (uint64_t)__builtin_popcountll(storage[w]);
    p->allocatable = freec;
    p->free_frames = freec;
    p->hint = next_clear(p, 0);
    p->initialized = true;
    return PMM_OK;
}

enum pmm_status pmm_instance_alloc(struct pmm *p, uint64_t count, uint64_t *out_addr)
{
    if (out_addr)
        *out_addr = 0;
    if (!p || !p->initialized || !out_addr)
        return PMM_ERR_NOT_INITIALIZED;
    p->alloc_calls++;
    if (count == 0 || count > p->frames) {
        p->alloc_failures++;
        return PMM_ERR_COUNT;
    }
    if (count > p->free_frames) {
        p->alloc_failures++;
        return PMM_ERR_NO_MEMORY;
    }

    uint64_t from = next_clear(p, p->hint);
    p->hint = from; /* all frames below `from` are used */
    uint64_t start = p->frames;
    while (from < p->frames && count <= p->frames - from) {
        uint64_t stop = next_set_lim(p, from, from + count); /* bounded: O(count/64) words, not O(run) */
        if (stop == from + count) {
            start = from;
            break;
        }
        from = next_clear(p, stop);
    }
    if (start >= p->frames) {
        p->alloc_failures++;
        return PMM_ERR_NO_MEMORY;
    }

    for (uint64_t i = 0; i < count; i++)
        bm_set(p, start + i);
    p->free_frames -= count;
    if (start == p->hint)
        p->hint = start + count;
    note_peak(p);
    *out_addr = start << PAGE_SHIFT;
    return PMM_OK;
}

enum pmm_status pmm_instance_free(struct pmm *p, uint64_t addr, uint64_t count)
{
    if (!p || !p->initialized)
        return PMM_ERR_NOT_INITIALIZED;
    p->free_calls++;

    enum pmm_status st = PMM_OK;
    uint64_t bytes, end;
    if (count == 0) {
        st = PMM_ERR_COUNT;
    } else if (addr & (PAGE_SIZE - 1)) {
        st = PMM_ERR_UNALIGNED;
    } else if (chk_mul_u64(count, PAGE_SIZE, &bytes) || chk_add_u64(addr, bytes, &end) ||
               end > MEMMAP_MAX_ADDR) {
        st = PMM_ERR_RANGE;
    } else {
        /* every byte of [addr,end) must be inside known USABLE regions */
        uint64_t cur = addr;
        while (cur < end) {
            const struct mem_region *r = memmap_find(&p->map, cur);
            if (!r) { st = PMM_ERR_RANGE; break; }
            if (r->type != MEM_USABLE) { st = PMM_ERR_PROTECTED; break; }
            cur = r->end < end ? r->end : end;
        }
        if (st == PMM_OK) {
            uint64_t f0 = addr >> PAGE_SHIFT, f1 = end >> PAGE_SHIFT;
            if (addr < PMM_LOW_LIMIT) {
                st = PMM_ERR_PROTECTED;
            } else if (p->prot_frames && f0 < p->prot_first + p->prot_frames && f1 > p->prot_first) {
                st = PMM_ERR_PROTECTED;
            } else {
                /* all frames must currently be allocated (checked before any change: atomic) */
                for (uint64_t f = f0; f < f1; f++) {
                    if (!bm_test(p, f)) {
                        st = PMM_ERR_NOT_ALLOCATED;
                        p->double_frees++;
                        break;
                    }
                }
                if (st == PMM_OK) {
                    for (uint64_t f = f0; f < f1; f++)
                        bm_clear(p, f);
                    p->free_frames += count;
                    if (f0 < p->hint)
                        p->hint = f0;
                }
            }
        }
    }
    if (st != PMM_OK)
        p->invalid_frees++;
    return st;
}

void pmm_instance_stats(const struct pmm *p, struct pmm_stats *s)
{
    memset(s, 0, sizeof *s);
    if (!p || !p->initialized)
        return;
    s->total_memory = 0;
    for (int t = 0; t < MEM_TYPE_COUNT; t++)
        s->total_memory += p->map.bytes[t];
    s->usable_memory = p->map.bytes[MEM_USABLE];
    s->reserved_memory = s->total_memory - s->usable_memory;
    s->total_frames = s->total_memory >> PAGE_SHIFT;
    s->allocatable_frames = p->allocatable;
    s->free_frames = p->free_frames;
    s->allocated_frames = p->allocatable - p->free_frames;
    s->peak_allocated_frames = p->peak_used;
    s->allocatable_memory = p->allocatable << PAGE_SHIFT;
    s->free_memory = p->free_frames << PAGE_SHIFT;
    s->allocated_memory = s->allocated_frames << PAGE_SHIFT;
    s->alloc_calls = p->alloc_calls;
    s->alloc_failures = p->alloc_failures;
    s->free_calls = p->free_calls;
    s->invalid_frees = p->invalid_frees;
    s->double_frees = p->double_frees;
    s->bitmap_bytes = p->words * 8;
    s->tracked_frames = p->frames;
}

bool pmm_instance_verify(const struct pmm *p)
{
    if (!p || !p->initialized)
        return false;
    uint64_t freec = 0;
    for (uint64_t w = 0; w < p->words; w++)
        freec += 64 - (uint64_t)__builtin_popcountll(p->bitmap[w]);
    if (freec != p->free_frames || p->free_frames > p->allocatable)
        return false;
    /* padding bits (beyond the last tracked frame) must stay set */
    if (p->frames & 63) {
        uint64_t pad = ALL_ONES << (p->frames & 63);
        if ((p->bitmap[p->words - 1] & pad) != pad)
            return false;
    }
    /* hint invariant */
    if (p->hint > p->frames || next_clear(p, 0) < p->hint)
        return false;
    /* protected frames used */
    for (uint64_t f = 0; f < p->prot_frames; f++)
        if (!bm_test(p, p->prot_first + f))
            return false;
    /* nothing free outside USABLE regions or below 1 MiB */
    for (uint64_t f = next_clear(p, 0); f < p->frames; f = next_clear(p, f + 1)) {
        const struct mem_region *r = memmap_find(&p->map, f << PAGE_SHIFT);
        if (!r || r->type != MEM_USABLE || (f << PAGE_SHIFT) < PMM_LOW_LIMIT)
            return false;
    }
    return true;
}

void pmm_instance_fragmentation(const struct pmm *p, uint64_t *runs, uint64_t *longest)
{
    uint64_t r = 0, best = 0;
    if (p && p->initialized) {
        for (uint64_t f = next_clear(p, 0); f < p->frames;) {
            uint64_t e = next_set(p, f);
            r++;
            if (e - f > best)
                best = e - f;
            f = next_clear(p, e);
        }
    }
    *runs = r;
    *longest = best;
}

bool pmm_instance_frame_used(const struct pmm *p, uint64_t addr)
{
    if (!p || !p->initialized)
        return false;
    uint64_t f = addr >> PAGE_SHIFT;
    return f < p->frames && bm_test(p, f);
}

/* ---- global API ---- */

static struct pmm g_pmm;
static uint64_t g_hhdm;

enum pmm_status pmm_init(const struct memmap *m, uint64_t hhdm_offset)
{
    if (g_pmm.initialized)
        return PMM_ERR_ALREADY_INITIALIZED;
    if (!m || m->count == 0 || m->usable_end == 0 || m->usable_end > MEMMAP_MAX_ADDR)
        return PMM_ERR_BAD_MAP;

    uint64_t words = ((m->usable_end >> PAGE_SHIFT) + 63) >> 6;
    uint64_t bytes = words * 8, need_bytes;
    if (chk_align_up_u64(bytes, PAGE_SIZE, &need_bytes))
        return PMM_ERR_BAD_MAP;
    uint64_t need = need_bytes >> PAGE_SHIFT;

    /* Bitmap placement: start of the first USABLE region (>= 1 MiB) that can hold it. */
    for (uint32_t i = 0; i < m->count; i++) {
        if (m->r[i].type != MEM_USABLE)
            continue;
        uint64_t b = m->r[i].base < PMM_LOW_LIMIT ? PMM_LOW_LIMIT : m->r[i].base;
        if (b >= m->r[i].end || m->r[i].end - b < need_bytes)
            continue;
        uint64_t virt;
        if (chk_add_u64(b, hhdm_offset, &virt))
            return PMM_ERR_BAD_MAP;
        enum pmm_status st = pmm_instance_init(&g_pmm, m, (uint64_t *)(uintptr_t)virt, words, b, need);
        if (st == PMM_OK)
            g_hhdm = hhdm_offset;
        return st;
    }
    return PMM_ERR_NO_STORAGE;
}

bool pmm_is_initialized(void)
{
    return g_pmm.initialized;
}

uint64_t pmm_alloc_pages(uint64_t count)
{
    uint64_t a;
    return pmm_instance_alloc(&g_pmm, count, &a) == PMM_OK ? a : 0;
}

uint64_t pmm_alloc_page(void)
{
    return pmm_alloc_pages(1);
}

enum pmm_status pmm_free_pages(uint64_t addr, uint64_t count)
{
    return pmm_instance_free(&g_pmm, addr, count);
}

enum pmm_status pmm_free_page(uint64_t addr)
{
    return pmm_instance_free(&g_pmm, addr, 1);
}

void pmm_get_stats(struct pmm_stats *s)
{
    pmm_instance_stats(&g_pmm, s);
}

struct pmm *pmm_global(void)
{
    return g_pmm.initialized ? &g_pmm : (struct pmm *)0;
}

uint64_t pmm_hhdm(void)
{
    return g_hhdm;
}

const char *pmm_status_name(enum pmm_status s)
{
    switch (s) {
    case PMM_OK:                     return "ok";
    case PMM_ERR_NOT_INITIALIZED:    return "not initialized";
    case PMM_ERR_ALREADY_INITIALIZED:return "already initialized";
    case PMM_ERR_BAD_MAP:            return "bad memory map";
    case PMM_ERR_NO_STORAGE:         return "no storage for bitmap";
    case PMM_ERR_UNALIGNED:          return "unaligned address";
    case PMM_ERR_RANGE:              return "address out of range";
    case PMM_ERR_PROTECTED:          return "protected frame";
    case PMM_ERR_NOT_ALLOCATED:      return "frame not allocated (double free)";
    case PMM_ERR_COUNT:              return "bad count";
    case PMM_ERR_NO_MEMORY:          return "out of memory";
    default:                         return "unknown";
    }
}
