/* ZERO KERNEL - Limine -> mem_raw_entry adapter. The only memory-map code that knows Limine. */

#include "limine.h"
#include "memory/memmap.h"

static enum mem_type classify(uint64_t t)
{
    switch (t) {
    case LIMINE_MEMMAP_USABLE:                 return MEM_USABLE;
    case LIMINE_MEMMAP_RESERVED:               return MEM_RESERVED;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return MEM_ACPI_RECLAIMABLE;
    case LIMINE_MEMMAP_ACPI_NVS:               return MEM_ACPI_NVS;
    case LIMINE_MEMMAP_BAD_MEMORY:             return MEM_BAD;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return MEM_BOOTLOADER_RECLAIMABLE;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return MEM_KERNEL;
    case LIMINE_MEMMAP_FRAMEBUFFER:            return MEM_FRAMEBUFFER;
    default:                                   return MEM_OTHER; /* incl. RESERVED_MAPPED, unknown */
    }
}

enum memmap_status memmap_raw_from_limine(const struct limine_memmap_response *resp,
                                          struct mem_raw_entry *out, uint64_t cap, uint64_t *n)
{
    if (!resp || !resp->entries || !out || !n)
        return MEMMAP_ERR_NULL;
    if (resp->entry_count == 0 || resp->entry_count > cap)
        return MEMMAP_ERR_COUNT;
    for (uint64_t i = 0; i < resp->entry_count; i++) {
        const struct limine_memmap_entry *e = resp->entries[i];
        if (!e)
            return MEMMAP_ERR_NULL;
        out[i].base = e->base;
        out[i].length = e->length;
        out[i].type = classify(e->type);
    }
    *n = resp->entry_count;
    return MEMMAP_OK;
}
