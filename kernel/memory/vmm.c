/* ZERO KERNEL - Virtual Memory Manager (Version 1.0.0 Core Foundation).
 *
 * 4-level x86_64 paging implementation.
 * Maps virtual pages to physical page frames with fine-grained memory protections:
 * W^X enforcement, NX bit handling, User/Kernel privilege separation, and HHDM retention.
 */

#include "memory/vmm.h"
#include "memory/pmm.h"
#include "memory/memmap.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"
#include "zero/checked.h"

/* Linker symbols for kernel section boundaries */
extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];
extern char __data_start[], __data_end[];
extern char __bss_start[], __bss_end[];
extern char __kernel_start[], __kernel_end[];

static bool vmm_initialized = false;
static uint64_t kernel_pml4_phys = 0;
static uint64_t *kernel_pml4_virt = NULL;
static uint64_t hhdm_base = 0;
static struct vmm_stats stats = { 0 };

static inline uint64_t pml4_idx(uint64_t v) { return (v >> 39) & 0x1FF; }
static inline uint64_t pdpt_idx(uint64_t v) { return (v >> 30) & 0x1FF; }
static inline uint64_t pd_idx(uint64_t v)   { return (v >> 21) & 0x1FF; }
static inline uint64_t pt_idx(uint64_t v)   { return (v >> 12) & 0x1FF; }

bool vmm_is_canonical(uint64_t virt)
{
    uint64_t high = virt >> 47;
    return (high == 0 || high == 0x1FFFF);
}

void *vmm_phys_to_virt(uint64_t phys)
{
    return (void *)(uintptr_t)(phys + hhdm_base);
}

uint64_t vmm_virt_to_phys_hhdm(const void *virt)
{
    uintptr_t va = (uintptr_t)virt;
    if (va < hhdm_base) {
        kpanic("vmm: virtual address %p is below HHDM base 0x%016lx", virt, hhdm_base);
    }
    return (uint64_t)(va - hhdm_base);
}

void vmm_invlpg(uint64_t virt)
{
    invlpg(virt);
    stats.tlb_flushes++;
}

void vmm_switch_pml4(uint64_t pml4_phys)
{
    write_cr3(pml4_phys);
    stats.cr3_switches++;
}

bool vmm_is_initialized(void)
{
    return vmm_initialized;
}

uint64_t vmm_get_kernel_pml4_phys(void)
{
    return kernel_pml4_phys;
}

uint64_t *vmm_get_kernel_pml4_virt(void)
{
    return kernel_pml4_virt;
}

void vmm_get_stats(struct vmm_stats *out_stats)
{
    if (out_stats) {
        *out_stats = stats;
    }
}

const char *vmm_status_name(enum vmm_status s)
{
    static const char *const names[] = {
        "OK",
        "NOT_INITIALIZED",
        "INVALID_ARG",
        "NON_CANONICAL",
        "UNALIGNED",
        "OUT_OF_MEMORY",
        "ALREADY_MAPPED",
        "NOT_MAPPED",
        "PAGE_FAULT"
    };
    return (s <= VMM_ERR_PAGE_FAULT) ? names[s] : "UNKNOWN_ERROR";
}

/* Allocate a zeroed 4 KiB frame for a page table level. */
static uint64_t alloc_table_frame(void)
{
    uint64_t phys = pmm_alloc_page();
    if (!phys)
        return 0;

    void *virt = vmm_phys_to_virt(phys);
    memset(virt, 0, VMM_PAGE_SIZE);
    stats.pt_frames_allocated++;
    return phys;
}

/* Ensure next level table exists; returns virtual pointer to that table or NULL on OOM. */
static uint64_t *get_or_create_table(uint64_t *parent_table, uint64_t idx, uint64_t flags)
{
    uint64_t entry = parent_table[idx];
    if (entry & VMM_FLAG_PRESENT) {
        uint64_t phys = entry & VMM_PTE_ADDR_MASK;
        return (uint64_t *)vmm_phys_to_virt(phys);
    }

    /* Table does not exist: allocate a new frame from PMM */
    uint64_t new_phys = alloc_table_frame();
    if (!new_phys)
        return NULL;

    /* Intermediate table entries must grant write and user if requested */
    uint64_t table_flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE;
    if (flags & VMM_FLAG_USER)
        table_flags |= VMM_FLAG_USER;

    parent_table[idx] = new_phys | table_flags;
    return (uint64_t *)vmm_phys_to_virt(new_phys);
}

enum vmm_status vmm_translate(const uint64_t *pml4, uint64_t virt, uint64_t *out_phys, uint64_t *out_flags)
{
    if (!pml4 || !vmm_is_canonical(virt))
        return VMM_ERR_INVALID_ARG;

    uint64_t pml4_e = pml4[pml4_idx(virt)];
    if (!(pml4_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    uint64_t *pdpt = (uint64_t *)vmm_phys_to_virt(pml4_e & VMM_PTE_ADDR_MASK);
    uint64_t pdpt_e = pdpt[pdpt_idx(virt)];
    if (!(pdpt_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    /* 1 GiB huge page check */
    if (pdpt_e & VMM_FLAG_HUGE) {
        if (out_phys)
            *out_phys = (pdpt_e & 0x000FFFFFC0000000ull) | (virt & 0x3FFFFFFFull);
        if (out_flags)
            *out_flags = pdpt_e & ~VMM_PTE_ADDR_MASK;
        return VMM_OK;
    }

    uint64_t *pd = (uint64_t *)vmm_phys_to_virt(pdpt_e & VMM_PTE_ADDR_MASK);
    uint64_t pd_e = pd[pd_idx(virt)];
    if (!(pd_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    /* 2 MiB huge page check */
    if (pd_e & VMM_FLAG_HUGE) {
        if (out_phys)
            *out_phys = (pd_e & 0x000FFFFFFFE00000ull) | (virt & 0x1FFFFFull);
        if (out_flags)
            *out_flags = pd_e & ~VMM_PTE_ADDR_MASK;
        return VMM_OK;
    }

    uint64_t *pt = (uint64_t *)vmm_phys_to_virt(pd_e & VMM_PTE_ADDR_MASK);
    uint64_t pt_e = pt[pt_idx(virt)];
    if (!(pt_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    if (out_phys)
        *out_phys = (pt_e & VMM_PTE_ADDR_MASK) | (virt & VMM_PAGE_MASK);
    if (out_flags)
        *out_flags = pt_e & ~VMM_PTE_ADDR_MASK;

    return VMM_OK;
}

enum vmm_status vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags)
{
    if (!pml4)
        return VMM_ERR_INVALID_ARG;
    if (!vmm_is_canonical(virt))
        return VMM_ERR_NON_CANONICAL;
    if ((virt & VMM_PAGE_MASK) != 0 || (phys & VMM_PAGE_MASK) != 0)
        return VMM_ERR_UNALIGNED;

    uint64_t *pdpt = get_or_create_table(pml4, pml4_idx(virt), flags);
    if (!pdpt)
        return VMM_ERR_OUT_OF_MEMORY;

    uint64_t *pd = get_or_create_table(pdpt, pdpt_idx(virt), flags);
    if (!pd)
        return VMM_ERR_OUT_OF_MEMORY;

    uint64_t *pt = get_or_create_table(pd, pd_idx(virt), flags);
    if (!pt)
        return VMM_ERR_OUT_OF_MEMORY;

    uint64_t idx = pt_idx(virt);
    if (pt[idx] & VMM_FLAG_PRESENT) {
        /* Page already mapped: overwrite or return ALREADY_MAPPED */
        pt[idx] = (phys & VMM_PTE_ADDR_MASK) | flags | VMM_FLAG_PRESENT;
        vmm_invlpg(virt);
        return VMM_OK;
    }

    pt[idx] = (phys & VMM_PTE_ADDR_MASK) | flags | VMM_FLAG_PRESENT;
    stats.pages_mapped++;
    vmm_invlpg(virt);
    return VMM_OK;
}

enum vmm_status vmm_map_pages(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t count, uint64_t flags)
{
    for (uint64_t i = 0; i < count; i++) {
        enum vmm_status s = vmm_map_page(pml4, virt + (i * VMM_PAGE_SIZE), phys + (i * VMM_PAGE_SIZE), flags);
        if (s != VMM_OK)
            return s;
    }
    return VMM_OK;
}

enum vmm_status vmm_unmap_page(uint64_t *pml4, uint64_t virt)
{
    if (!pml4 || !vmm_is_canonical(virt))
        return VMM_ERR_INVALID_ARG;
    if ((virt & VMM_PAGE_MASK) != 0)
        return VMM_ERR_UNALIGNED;

    uint64_t pml4_e = pml4[pml4_idx(virt)];
    if (!(pml4_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    uint64_t *pdpt = (uint64_t *)vmm_phys_to_virt(pml4_e & VMM_PTE_ADDR_MASK);
    uint64_t pdpt_e = pdpt[pdpt_idx(virt)];
    if (!(pdpt_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    uint64_t *pd = (uint64_t *)vmm_phys_to_virt(pdpt_e & VMM_PTE_ADDR_MASK);
    uint64_t pd_e = pd[pd_idx(virt)];
    if (!(pd_e & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    uint64_t *pt = (uint64_t *)vmm_phys_to_virt(pd_e & VMM_PTE_ADDR_MASK);
    uint64_t idx = pt_idx(virt);
    if (!(pt[idx] & VMM_FLAG_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    pt[idx] = 0;
    stats.pages_unmapped++;
    if (stats.pages_mapped > 0)
        stats.pages_mapped--;

    vmm_invlpg(virt);
    return VMM_OK;
}

enum vmm_status vmm_unmap_pages(uint64_t *pml4, uint64_t virt, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++) {
        enum vmm_status s = vmm_unmap_page(pml4, virt + (i * VMM_PAGE_SIZE));
        if (s != VMM_OK && s != VMM_ERR_NOT_MAPPED)
            return s;
    }
    return VMM_OK;
}

enum vmm_status vmm_create_address_space(uint64_t *out_pml4_phys, uint64_t **out_pml4_virt)
{
    if (!out_pml4_phys || !out_pml4_virt)
        return VMM_ERR_INVALID_ARG;

    uint64_t p_phys = alloc_table_frame();
    if (!p_phys)
        return VMM_ERR_OUT_OF_MEMORY;

    uint64_t *p_virt = (uint64_t *)vmm_phys_to_virt(p_phys);

    /* Clone kernel mappings from higher-half (entries 256 to 511) */
    for (int i = 256; i < 512; i++) {
        p_virt[i] = kernel_pml4_virt[i];
    }

    *out_pml4_phys = p_phys;
    *out_pml4_virt = p_virt;
    return VMM_OK;
}

enum vmm_status vmm_destroy_address_space(uint64_t pml4_phys)
{
    if (!pml4_phys || pml4_phys == kernel_pml4_phys)
        return VMM_ERR_INVALID_ARG;

    uint64_t *pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);

    /* Free all lower-half user tables (entries 0 to 255) */
    for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
        if (!(pml4[pml4_i] & VMM_FLAG_PRESENT))
            continue;

        uint64_t pdpt_phys = pml4[pml4_i] & VMM_PTE_ADDR_MASK;
        uint64_t *pdpt = (uint64_t *)vmm_phys_to_virt(pdpt_phys);

        for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            if (!(pdpt[pdpt_i] & VMM_FLAG_PRESENT) || (pdpt[pdpt_i] & VMM_FLAG_HUGE))
                continue;

            uint64_t pd_phys = pdpt[pdpt_i] & VMM_PTE_ADDR_MASK;
            uint64_t *pd = (uint64_t *)vmm_phys_to_virt(pd_phys);

            for (int pd_i = 0; pd_i < 512; pd_i++) {
                if (!(pd[pd_i] & VMM_FLAG_PRESENT) || (pd[pd_i] & VMM_FLAG_HUGE))
                    continue;

                uint64_t pt_phys = pd[pd_i] & VMM_PTE_ADDR_MASK;
                pmm_free_page(pt_phys);
                stats.pt_frames_allocated--;
            }
            pmm_free_page(pd_phys);
            stats.pt_frames_allocated--;
        }
        pmm_free_page(pdpt_phys);
        stats.pt_frames_allocated--;
    }

    pmm_free_page(pml4_phys);
    stats.pt_frames_allocated--;
    return VMM_OK;
}

enum vmm_status vmm_init(const struct boot_info *boot)
{
    if (!boot || !boot->hhdm_offset)
        return VMM_ERR_INVALID_ARG;

    hhdm_base = boot->hhdm_offset;
    KLOG_INFO("vmm", "initializing Virtual Memory Manager (HHDM: 0x%016lx)", hhdm_base);

    /* 1. Allocate root PML4 frame for the kernel */
    kernel_pml4_phys = alloc_table_frame();
    if (!kernel_pml4_phys) {
        kpanic("vmm: failed to allocate kernel root PML4");
    }
    kernel_pml4_virt = (uint64_t *)vmm_phys_to_virt(kernel_pml4_phys);

    /* 2. Map all usable RAM regions into the HHDM */
    struct pmm_stats ps;
    pmm_get_stats(&ps);
    uint64_t total_mapped_frames = ps.tracked_frames;

    /*
     * Limine may enter the kernel with its active stack outside the highest
     * USABLE region tracked by PMM (for example, in BOOTLOADER_RECLAIMABLE
     * memory). The new PML4 must preserve that stack mapping before CR3 is
     * switched, otherwise the first interrupt/stack access becomes a #PF
     * followed by a #DF.
     *
     * Keep a 2 MiB HHDM window above the current boot stack mapped as well.
     * The lower portion is already covered by the normal 0..tracked_frames
     * mapping. We intentionally map only this small additional window rather
     * than every reserved physical region in the machine.
     */
    uint64_t current_rsp = 0;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(current_rsp));
    if (current_rsp >= hhdm_base) {
        uint64_t stack_phys = current_rsp - hhdm_base;
        uint64_t stack_end = (stack_phys + (2ull * 1024 * 1024) + VMM_PAGE_SIZE - 1)
                             & ~(VMM_PAGE_SIZE - 1);
        uint64_t stack_frames = stack_end / VMM_PAGE_SIZE;
        if (stack_frames > total_mapped_frames)
            total_mapped_frames = stack_frames;
        KLOG_INFO("vmm", "preserving active boot stack at HHDM physical 0x%016lx",
                  stack_phys);
    } else {
        KLOG_WARN("vmm", "active stack is outside HHDM; retaining PMM-only direct map");
    }

    KLOG_INFO("vmm", "mapping %lu physical frames into HHDM window", total_mapped_frames);

    /* Map PMM-tracked RAM plus the active boot-stack window with Read-Write-NX. */
    for (uint64_t frame = 0; frame < total_mapped_frames; frame++) {
        uint64_t phys = frame * VMM_PAGE_SIZE;
        uint64_t virt = hhdm_base + phys;
        enum vmm_status s = vmm_map_page(kernel_pml4_virt, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_NX);
        if (s != VMM_OK) {
            kpanic("vmm: failed to map HHDM page 0x%016lx -> 0x%016lx: %s", virt, phys, vmm_status_name(s));
        }
    }

    /* 3. Map Kernel ELF sections with strict W^X permissions */
    uint64_t k_phys_base = boot->kernel_phys_base;
    uint64_t k_virt_base = (uint64_t)(uintptr_t)__kernel_start;

    /* Text: Read-Only + Executable (NX=0) */
    uint64_t text_v_start = (uint64_t)(uintptr_t)__text_start;
    uint64_t text_v_end   = (uint64_t)(uintptr_t)__text_end;
    for (uint64_t va = text_v_start; va < text_v_end; va += VMM_PAGE_SIZE) {
        uint64_t pa = k_phys_base + (va - k_virt_base);
        vmm_map_page(kernel_pml4_virt, va, pa, 0); /* Executable, read-only */
    }
    KLOG_INFO("vmm", "mapped kernel .text [0x%016lx - 0x%016lx] R-X", text_v_start, text_v_end);

    /* Rodata: Read-Only + Non-Executable (NX=1, WRITABLE=0) */
    uint64_t rodata_v_start = (uint64_t)(uintptr_t)__rodata_start;
    uint64_t rodata_v_end   = (uint64_t)(uintptr_t)__rodata_end;
    for (uint64_t va = rodata_v_start; va < rodata_v_end; va += VMM_PAGE_SIZE) {
        uint64_t pa = k_phys_base + (va - k_virt_base);
        vmm_map_page(kernel_pml4_virt, va, pa, VMM_FLAG_NX);
    }
    KLOG_INFO("vmm", "mapped kernel .rodata [0x%016lx - 0x%016lx] R--", rodata_v_start, rodata_v_end);

    /* Data + BSS: Read-Write + Non-Executable (NX=1, WRITABLE=1) */
    uint64_t data_v_start = (uint64_t)(uintptr_t)__data_start;
    uint64_t bss_v_end    = (uint64_t)(uintptr_t)__bss_end;
    for (uint64_t va = data_v_start; va < bss_v_end; va += VMM_PAGE_SIZE) {
        uint64_t pa = k_phys_base + (va - k_virt_base);
        vmm_map_page(kernel_pml4_virt, va, pa, VMM_FLAG_WRITABLE | VMM_FLAG_NX);
    }
    KLOG_INFO("vmm", "mapped kernel .data/.bss [0x%016lx - 0x%016lx] RW-", data_v_start, bss_v_end);

    /* 4. Framebuffer mapping (if available) */
    if (boot->fb_phys && boot->fb_size) {
        uint64_t fb_pages = (boot->fb_size + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE;
        uint64_t fb_virt = hhdm_base + boot->fb_phys;
        vmm_map_pages(kernel_pml4_virt, fb_virt, boot->fb_phys, fb_pages,
                      VMM_FLAG_WRITABLE | VMM_FLAG_NX | VMM_FLAG_NOCACHE);
        KLOG_INFO("vmm", "mapped framebuffer 0x%016lx (%lu KiB) at 0x%016lx",
                  boot->fb_phys, boot->fb_size / 1024, fb_virt);
    }

    /* 5. Switch to ZERO OS authoritative PML4 */
    KLOG_INFO("vmm", "switching CR3 to kernel PML4 (0x%016lx)", kernel_pml4_phys);
    vmm_switch_pml4(kernel_pml4_phys);

    vmm_initialized = true;
    KLOG_INFO("vmm", "VMM initialization complete: %lu page-table frames allocated", stats.pt_frames_allocated);
    return VMM_OK;
}
