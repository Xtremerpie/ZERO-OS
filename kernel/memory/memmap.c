/* ZERO KERNEL - memory-map normalization. See memmap.h for the rules. */

#include "memory/memmap.h"
#include "zero/checked.h"
#include "zero/string.h"

static int type_priority(enum mem_type t)
{
    switch (t) {
    case MEM_KERNEL:                 return 9;
    case MEM_FRAMEBUFFER:            return 8;
    case MEM_ACPI_NVS:               return 7;
    case MEM_RESERVED:               return 6;
    case MEM_OTHER:                  return 6;
    case MEM_BAD:                    return 5;
    case MEM_ACPI_RECLAIMABLE:       return 4;
    case MEM_BOOTLOADER_RECLAIMABLE: return 3;
    case MEM_USABLE:                 return 1;
    default:                         return 6; /* unreachable: type validated */
    }
}

enum memmap_status memmap_build(struct memmap *out, const struct mem_raw_entry *in, uint64_t n)
{
    struct mem_raw_entry tmp[MEMMAP_MAX_RAW];
    uint64_t bounds[2 * MEMMAP_MAX_RAW];

    if (!out || !in)
        return MEMMAP_ERR_NULL;
    memset(out, 0, sizeof *out);
    if (n == 0 || n > MEMMAP_MAX_RAW)
        return MEMMAP_ERR_COUNT;

    /* 1. copy, validate each entry independently, drop empty ones */
    uint32_t k = 0;
    for (uint64_t i = 0; i < n; i++) {
        uint64_t end;
        if (in[i].length == 0)
            continue;
        if ((unsigned)in[i].type >= MEM_TYPE_COUNT)
            return MEMMAP_ERR_RANGE;
        if (chk_add_u64(in[i].base, in[i].length, &end))
            return MEMMAP_ERR_OVERFLOW;
        if (end > MEMMAP_MAX_ADDR)
            return MEMMAP_ERR_RANGE;
        tmp[k++] = in[i];
    }
    if (k == 0)
        return MEMMAP_ERR_NO_USABLE;

    /* 2. insertion sort by base (n <= 128) */
    for (uint32_t i = 1; i < k; i++) {
        struct mem_raw_entry key = tmp[i];
        uint32_t j = i;
        while (j > 0 && tmp[j - 1].base > key.base) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = key;
    }

    /* 3. overlap rejection (lengths already proven not to wrap) */
    for (uint32_t i = 1; i < k; i++)
        if (tmp[i].base < tmp[i - 1].base + tmp[i - 1].length)
            return MEMMAP_ERR_OVERLAP;

    /* 4. page alignment: usable shrinks inward, everything else expands outward.
     *    Values are <= 2^52 + 4095, so no arithmetic can wrap here. */
    uint32_t m = 0;
    for (uint32_t i = 0; i < k; i++) {
        uint64_t b = tmp[i].base, e = tmp[i].base + tmp[i].length;
        if (tmp[i].type == MEM_USABLE) {
            b = (b + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            e = align_down_u64(e, PAGE_SIZE);
            if (e <= b)
                continue;
        } else {
            b = align_down_u64(b, PAGE_SIZE);
            e = (e + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        }
        tmp[m].base = b;
        tmp[m].length = e - b;
        tmp[m].type = tmp[i].type;
        m++;
    }

    /* 5. elementary intervals between all boundaries; highest-priority type wins */
    uint32_t nb = 0;
    for (uint32_t i = 0; i < m; i++) {
        bounds[nb++] = tmp[i].base;
        bounds[nb++] = tmp[i].base + tmp[i].length;
    }
    for (uint32_t i = 1; i < nb; i++) {
        uint64_t key = bounds[i];
        uint32_t j = i;
        while (j > 0 && bounds[j - 1] > key) {
            bounds[j] = bounds[j - 1];
            j--;
        }
        bounds[j] = key;
    }

    for (uint32_t b = 0; b + 1 < nb; b++) {
        uint64_t lo = bounds[b], hi = bounds[b + 1];
        if (lo == hi)
            continue;
        int best = 0;
        enum mem_type bt = MEM_USABLE;
        for (uint32_t i = 0; i < m; i++) {
            if (tmp[i].base <= lo && hi <= tmp[i].base + tmp[i].length) {
                int p = type_priority(tmp[i].type);
                if (p > best) {
                    best = p;
                    bt = tmp[i].type;
                }
            }
        }
        if (!best)
            continue; /* hole */
        if (out->count && out->r[out->count - 1].end == lo && out->r[out->count - 1].type == bt) {
            out->r[out->count - 1].end = hi;
        } else {
            if (out->count >= MEMMAP_MAX_REGIONS) {
                memset(out, 0, sizeof *out);
                return MEMMAP_ERR_TOO_MANY;
            }
            out->r[out->count].base = lo;
            out->r[out->count].end = hi;
            out->r[out->count].type = bt;
            out->count++;
        }
    }

    for (uint32_t i = 0; i < out->count; i++) {
        out->bytes[out->r[i].type] += out->r[i].end - out->r[i].base;
        if (out->r[i].type == MEM_USABLE)
            out->usable_end = out->r[i].end;
    }
    if (out->bytes[MEM_USABLE] == 0) {
        memset(out, 0, sizeof *out);
        return MEMMAP_ERR_NO_USABLE;
    }
    return MEMMAP_OK;
}

const struct mem_region *memmap_find(const struct memmap *m, uint64_t addr)
{
    uint32_t lo = 0, hi = m->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (addr < m->r[mid].base)
            hi = mid;
        else if (addr >= m->r[mid].end)
            lo = mid + 1;
        else
            return &m->r[mid];
    }
    return NULL;
}

const char *memmap_type_name(enum mem_type t)
{
    switch (t) {
    case MEM_USABLE:                 return "usable";
    case MEM_RESERVED:               return "reserved";
    case MEM_ACPI_RECLAIMABLE:       return "acpi-reclaimable";
    case MEM_ACPI_NVS:               return "acpi-nvs";
    case MEM_BAD:                    return "bad";
    case MEM_BOOTLOADER_RECLAIMABLE: return "bootloader-reclaimable";
    case MEM_KERNEL:                 return "kernel+modules";
    case MEM_FRAMEBUFFER:            return "framebuffer";
    case MEM_OTHER:                  return "other-reserved";
    default:                         return "invalid";
    }
}

const char *memmap_status_name(enum memmap_status s)
{
    switch (s) {
    case MEMMAP_OK:             return "ok";
    case MEMMAP_ERR_NULL:       return "null pointer";
    case MEMMAP_ERR_COUNT:      return "bad entry count";
    case MEMMAP_ERR_OVERFLOW:   return "address arithmetic overflow";
    case MEMMAP_ERR_RANGE:      return "address or type out of range";
    case MEMMAP_ERR_OVERLAP:    return "overlapping entries";
    case MEMMAP_ERR_TOO_MANY:   return "too many normalized regions";
    case MEMMAP_ERR_NO_USABLE:  return "no usable memory";
    default:                    return "unknown";
    }
}
