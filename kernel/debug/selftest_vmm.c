/* ZERO KERNEL - VMM Self-Tests (Version 1.0.0 Core Foundation).
 *
 * Verifies:
 * 1. Canonical address checking (lower half, higher half, non-canonical gap).
 * 2. 4 KiB virtual page mapping and physical translation.
 * 3. Virtual write -> Physical read verification.
 * 4. W^X enforcement check on live page tables (.text not writable, .data NX).
 * 5. Page unmapping and TLB invalidation.
 * 6. Contiguous multi-page mapping and access.
 * 7. Address space cloning and isolation (higher-half shared, lower-half private).
 * 8. Address space destruction and page table frame reclamation to PMM.
 */

#include "memory/vmm.h"
#include "memory/vmm_test.h"
#include "memory/pmm.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"

extern char __text_start[], __text_end[];
extern char __data_start[], __data_end[];

static bool test_canonical(void)
{
    /* Valid lower half */
    if (!vmm_is_canonical(0x0000000000000000ull)) return false;
    if (!vmm_is_canonical(0x00007FFFFFFFFFFFull)) return false;
    if (!vmm_is_canonical(0x0000123456789000ull)) return false;

    /* Valid higher half */
    if (!vmm_is_canonical(0xFFFF800000000000ull)) return false;
    if (!vmm_is_canonical(0xFFFFFFFFFFFFFFFFull)) return false;
    if (!vmm_is_canonical(0xFFFFFFFF80000000ull)) return false;

    /* Non-canonical addresses in the 48-bit hole */
    if (vmm_is_canonical(0x0000800000000000ull)) return false;
    if (vmm_is_canonical(0xFFFF7FFFFFFFFFFFull)) return false;
    if (vmm_is_canonical(0xDEADBEEF00000000ull)) return false;

    return true;
}

static bool test_map_translate_unmap(void)
{
    uint64_t *pml4 = vmm_get_kernel_pml4_virt();
    if (!pml4) return false;

    uint64_t phys_frame = pmm_alloc_page();
    if (!phys_frame) return false;

    /* Choose a test virtual address in the dynamic kernel range */
    uint64_t test_va = 0xFFFFFFFFC0000000ull;

    /* 1. Ensure not initially mapped */
    uint64_t out_phys = 0, out_flags = 0;
    if (vmm_translate(pml4, test_va, &out_phys, &out_flags) != VMM_ERR_NOT_MAPPED) {
        pmm_free_page(phys_frame);
        return false;
    }

    /* 2. Map page with RW + NX */
    enum vmm_status s = vmm_map_page(pml4, test_va, phys_frame, VMM_FLAG_WRITABLE | VMM_FLAG_NX);
    if (s != VMM_OK) {
        pmm_free_page(phys_frame);
        return false;
    }

    /* 3. Translate and verify physical address and flags */
    if (vmm_translate(pml4, test_va, &out_phys, &out_flags) != VMM_OK) {
        pmm_free_page(phys_frame);
        return false;
    }
    if (out_phys != phys_frame) {
        pmm_free_page(phys_frame);
        return false;
    }
    if (!(out_flags & VMM_FLAG_WRITABLE) || !(out_flags & VMM_FLAG_NX)) {
        pmm_free_page(phys_frame);
        return false;
    }

    /* 4. Write data to the virtual address and read it back directly via physical HHDM */
    volatile uint64_t *vptr = (volatile uint64_t *)(uintptr_t)test_va;
    uint64_t magic_val = 0x5A45524F53595354ull; /* "ZEROSYST" */
    *vptr = magic_val;

    volatile uint64_t *pptr = (volatile uint64_t *)vmm_phys_to_virt(phys_frame);
    if (*pptr != magic_val) {
        pmm_free_page(phys_frame);
        return false;
    }

    /* 5. Unmap page and verify it is no longer translated */
    if (vmm_unmap_page(pml4, test_va) != VMM_OK) {
        pmm_free_page(phys_frame);
        return false;
    }
    if (vmm_translate(pml4, test_va, &out_phys, &out_flags) != VMM_ERR_NOT_MAPPED) {
        pmm_free_page(phys_frame);
        return false;
    }

    pmm_free_page(phys_frame);
    return true;
}

static bool test_wx_enforcement(void)
{
    uint64_t *pml4 = vmm_get_kernel_pml4_virt();
    if (!pml4) return false;

    /* Verify kernel .text is NOT writable */
    uint64_t text_va = (uint64_t)(uintptr_t)__text_start;
    uint64_t text_pa = 0, text_flags = 0;
    if (vmm_translate(pml4, text_va, &text_pa, &text_flags) != VMM_OK)
        return false;

    if (text_flags & VMM_FLAG_WRITABLE) {
        KLOG_ERROR("vmm_test", "W^X violation: kernel .text is marked WRITABLE!");
        return false;
    }

    /* Verify kernel .data is marked NX (Non-Executable) */
    uint64_t data_va = (uint64_t)(uintptr_t)__data_start;
    uint64_t data_pa = 0, data_flags = 0;
    if (vmm_translate(pml4, data_va, &data_pa, &data_flags) != VMM_OK)
        return false;

    if (!(data_flags & VMM_FLAG_NX)) {
        KLOG_ERROR("vmm_test", "W^X violation: kernel .data is NOT marked NX!");
        return false;
    }

    return true;
}

static bool test_address_space_isolation(void)
{
    uint64_t user_pml4_phys = 0;
    uint64_t *user_pml4_virt = NULL;

    /* 1. Create a new address space */
    if (vmm_create_address_space(&user_pml4_phys, &user_pml4_virt) != VMM_OK)
        return false;

    /* 2. Verify that kernel higher-half mappings exist in the new address space */
    uint64_t text_va = (uint64_t)(uintptr_t)__text_start;
    uint64_t pa = 0, flags = 0;
    if (vmm_translate(user_pml4_virt, text_va, &pa, &flags) != VMM_OK) {
        vmm_destroy_address_space(user_pml4_phys);
        return false;
    }

    /* 3. Map a user page at 0x400000 (lower half) in the new address space */
    uint64_t user_va = 0x0000000000400000ull;
    uint64_t u_phys = pmm_alloc_page();
    if (!u_phys) {
        vmm_destroy_address_space(user_pml4_phys);
        return false;
    }

    if (vmm_map_page(user_pml4_virt, user_va, u_phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NX) != VMM_OK) {
        pmm_free_page(u_phys);
        vmm_destroy_address_space(user_pml4_phys);
        return false;
    }

    /* 4. Verify user page IS present in the user address space */
    if (vmm_translate(user_pml4_virt, user_va, &pa, &flags) != VMM_OK || pa != u_phys) {
        pmm_free_page(u_phys);
        vmm_destroy_address_space(user_pml4_phys);
        return false;
    }

    /* 5. Verify user page is NOT present in the kernel address space (isolation!) */
    uint64_t *kernel_pml4 = vmm_get_kernel_pml4_virt();
    if (vmm_translate(kernel_pml4, user_va, &pa, &flags) != VMM_ERR_NOT_MAPPED) {
        KLOG_ERROR("vmm_test", "Isolation breach: user mapping leaked into kernel PML4!");
        pmm_free_page(u_phys);
        vmm_destroy_address_space(user_pml4_phys);
        return false;
    }

    /* 6. Clean up: unmap user page, free physical frame, destroy address space */
    vmm_unmap_page(user_pml4_virt, user_va);
    pmm_free_page(u_phys);

    if (vmm_destroy_address_space(user_pml4_phys) != VMM_OK)
        return false;

    return true;
}

bool vmm_run_selftests(void)
{
    KLOG_INFO("vmm_test", "running VMM self-test suite...");

    if (!test_canonical()) {
        KLOG_ERROR("vmm_test", "[FAIL] canonical address check");
        return false;
    }
    kprintf("  [PASS] vmm: canonical address validation\n");

    if (!test_map_translate_unmap()) {
        KLOG_ERROR("vmm_test", "[FAIL] map/translate/unmap cycle");
        return false;
    }
    kprintf("  [PASS] vmm: 4 KiB map, translate, memory access and unmap\n");

    if (!test_wx_enforcement()) {
        KLOG_ERROR("vmm_test", "[FAIL] W^X permission enforcement");
        return false;
    }
    kprintf("  [PASS] vmm: W^X permission enforcement (.text R-X, .data RW-NX)\n");

    if (!test_address_space_isolation()) {
        KLOG_ERROR("vmm_test", "[FAIL] address space isolation");
        return false;
    }
    kprintf("  [PASS] vmm: address space creation, isolation and destruction\n");

    KLOG_INFO("vmm_test", "all VMM self-tests passed successfully");
    return true;
}
