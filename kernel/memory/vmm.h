/* ZERO KERNEL - Virtual Memory Manager (Version 1.0.0 Core Foundation).
 *
 * 4-level x86_64 paging (PML4, PDPT, PD, PT) with 4 KiB granular mappings,
 * W^X permission enforcement, canonical address validation, TLB invalidation,
 * and higher-half address space management.
 *
 * All physical frames for page tables are allocated through the PMM (pmm_alloc_page)
 * and reached in kernel virtual space via the HHDM (Higher-Half Direct Map).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "zero/bootinfo.h"

#define VMM_PAGE_SIZE 4096ull
#define VMM_PAGE_MASK 0xFFFull
#define VMM_PAGE_SHIFT 12

/* x86_64 Page Table Entry (PTE) architectural flag bits */
#define VMM_FLAG_PRESENT       (1ull << 0)
#define VMM_FLAG_WRITABLE      (1ull << 1)
#define VMM_FLAG_USER          (1ull << 2)
#define VMM_FLAG_WRITETHROUGH  (1ull << 3)
#define VMM_FLAG_NOCACHE       (1ull << 4)
#define VMM_FLAG_ACCESSED      (1ull << 5)
#define VMM_FLAG_DIRTY         (1ull << 6)
#define VMM_FLAG_HUGE          (1ull << 7)
#define VMM_FLAG_GLOBAL        (1ull << 8)
#define VMM_FLAG_NX            (1ull << 63)

#define VMM_PTE_ADDR_MASK      0x000FFFFFFFFFF000ull

/* Status codes */
enum vmm_status {
    VMM_OK = 0,
    VMM_ERR_NOT_INITIALIZED,
    VMM_ERR_INVALID_ARG,
    VMM_ERR_NON_CANONICAL,
    VMM_ERR_UNALIGNED,
    VMM_ERR_OUT_OF_MEMORY,
    VMM_ERR_ALREADY_MAPPED,
    VMM_ERR_NOT_MAPPED,
    VMM_ERR_PAGE_FAULT
};

struct vmm_stats {
    uint64_t pt_frames_allocated; /* Total 4 KiB frames used for page tables */
    uint64_t pages_mapped;        /* Total active 4 KiB virtual pages mapped */
    uint64_t pages_unmapped;
    uint64_t tlb_flushes;
    uint64_t cr3_switches;
};

/* ---- Core VMM API ---- */

/* Initialize VMM, construct the kernel's authoritative page table hierarchy,
 * map HHDM, kernel image with W^X enforcement, framebuffer, and switch CR3. */
enum vmm_status vmm_init(const struct boot_info *boot);

/* Returns true if the kernel's VMM has been initialized and CR3 switched. */
bool vmm_is_initialized(void);

/* Returns the physical address of the active kernel PML4. */
uint64_t vmm_get_kernel_pml4_phys(void);

/* Returns the virtual pointer to the active kernel PML4 (via HHDM). */
uint64_t *vmm_get_kernel_pml4_virt(void);

/* Translate a virtual address to physical using the given PML4 table.
 * If out_flags is non-NULL, receives the combined permission flags. */
enum vmm_status vmm_translate(const uint64_t *pml4, uint64_t virt, uint64_t *out_phys, uint64_t *out_flags);

/* Map a single 4 KiB virtual page to a physical frame. */
enum vmm_status vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);

/* Map a contiguous sequence of 4 KiB pages. */
enum vmm_status vmm_map_pages(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t count, uint64_t flags);

/* Unmap a single 4 KiB virtual page and flush TLB. */
enum vmm_status vmm_unmap_page(uint64_t *pml4, uint64_t virt);

/* Unmap a contiguous range of virtual pages. */
enum vmm_status vmm_unmap_pages(uint64_t *pml4, uint64_t virt, uint64_t count);

/* Create a new address space (clones higher-half kernel tables, leaves lower-half empty). */
enum vmm_status vmm_create_address_space(uint64_t *out_pml4_phys, uint64_t **out_pml4_virt);

/* Destroy an address space (frees all lower-half page table frames to PMM). */
enum vmm_status vmm_destroy_address_space(uint64_t pml4_phys);

/* Switch the hardware CR3 register to a new PML4 table. */
void vmm_switch_pml4(uint64_t pml4_phys);

/* Invalidate a single TLB entry. */
void vmm_invlpg(uint64_t virt);

/* Physical to virtual conversion via the HHDM. */
void *vmm_phys_to_virt(uint64_t phys);

/* Virtual to physical conversion for an address within the HHDM window. */
uint64_t vmm_virt_to_phys_hhdm(const void *virt);

/* Validate whether an address is 48-bit canonical. */
bool vmm_is_canonical(uint64_t virt);

/* Retrieve VMM operational statistics. */
void vmm_get_stats(struct vmm_stats *out_stats);

/* Friendly status string for logging/debugging. */
const char *vmm_status_name(enum vmm_status status);
