/* ZERO KERNEL - ELF64 Executable Loader Implementation (Version 2.0.0 System Architecture). */

#include "loader/elf.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "zero/string.h"
#include "zero/log.h"
#include "zero/checked.h"

enum elf_load_status elf_validate(const void *image, size_t size)
{
    if (!image || size < sizeof(elf64_ehdr_t))
        return ELF_ERR_INVALID_HEADER;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)image;

    /* Verify ELF magic 0x7F 'E' 'L' 'F' */
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3) {
        return ELF_ERR_INVALID_HEADER;
    }

    /* Verify 64-bit architecture */
    if (ehdr->e_ident[4] != ELFCLASS64)
        return ELF_ERR_NOT_64BIT;

    /* Verify Little-Endian */
    if (ehdr->e_ident[5] != ELFDATA2LSB)
        return ELF_ERR_INVALID_HEADER;

    /* Verify Machine = x86_64 */
    if (ehdr->e_machine != EM_X86_64)
        return ELF_ERR_NOT_X86_64;

    /* Verify Executable or Shared/Position-Independent */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
        return ELF_ERR_NOT_EXECUTABLE;

    /* Check program headers bounds */
    uint64_t ph_end;
    if (chk_mul_u64((uint64_t)ehdr->e_phnum, (uint64_t)ehdr->e_phentsize, &ph_end))
        return ELF_ERR_INVALID_HEADER;
    if (chk_add_u64(ehdr->e_phoff, ph_end, &ph_end))
        return ELF_ERR_INVALID_HEADER;
    if (ph_end > size)
        return ELF_ERR_INVALID_HEADER;

    /* Check each PT_LOAD segment */
    const uint8_t *base = (const uint8_t *)image;
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf64_phdr_t *phdr = (const elf64_phdr_t *)(base + ehdr->e_phoff + (i * ehdr->e_phentsize));
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_filesz > phdr->p_memsz)
                return ELF_ERR_BAD_SEGMENT;

            uint64_t file_end;
            if (chk_add_u64(phdr->p_offset, phdr->p_filesz, &file_end))
                return ELF_ERR_BAD_SEGMENT;
            if (file_end > size)
                return ELF_ERR_BAD_SEGMENT;

            uint64_t mem_end;
            if (chk_add_u64(phdr->p_vaddr, phdr->p_memsz, &mem_end))
                return ELF_ERR_BAD_SEGMENT;

            /* Segments must be in user lower-half memory */
            if (mem_end > 0x00007FFFFFFFFFFFull)
                return ELF_ERR_BAD_SEGMENT;
        }
    }

    return ELF_OK;
}

enum elf_load_status elf_load(process_t *proc, const void *image, size_t size, uint64_t *out_entry, uint64_t *out_stack)
{
    if (!proc || !proc->pml4_virt || !image)
        return ELF_ERR_INVALID_HEADER;

    enum elf_load_status status = elf_validate(image, size);
    if (status != ELF_OK)
        return status;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)image;
    const uint8_t *base = (const uint8_t *)image;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf64_phdr_t *phdr = (const elf64_phdr_t *)(base + ehdr->e_phoff + (i * ehdr->e_phentsize));
        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0)
            continue;

        uint64_t vaddr_start = phdr->p_vaddr & ~(VMM_PAGE_SIZE - 1);
        uint64_t vaddr_end = (phdr->p_vaddr + phdr->p_memsz + VMM_PAGE_SIZE - 1) & ~(VMM_PAGE_SIZE - 1);

        uint64_t page_flags = VMM_FLAG_PRESENT | VMM_FLAG_USER;
        if (phdr->p_flags & PF_W) {
            page_flags |= VMM_FLAG_WRITABLE;
        }
        if (!(phdr->p_flags & PF_X)) {
            page_flags |= VMM_FLAG_NX;
        }

        /* Map each page in the segment */
        for (uint64_t page_va = vaddr_start; page_va < vaddr_end; page_va += VMM_PAGE_SIZE) {
            uint64_t phys = 0;
            enum vmm_status vs = vmm_translate(proc->pml4_virt, page_va, &phys, NULL);
            if (vs != VMM_OK) {
                phys = pmm_alloc_page();
                if (!phys) {
                    KLOG_ERR("elf", "Out of physical memory loading segment %u", i);
                    return ELF_ERR_NO_MEMORY;
                }

                memset(vmm_phys_to_virt(phys), 0, VMM_PAGE_SIZE);

                vs = vmm_map_page(proc->pml4_virt, page_va, phys, page_flags);
                if (vs != VMM_OK) {
                    KLOG_ERR("elf", "Failed to map page 0x%016llx: %d", page_va, vs);
                    return ELF_ERR_MAP_FAILED;
                }
                proc->memory_bytes_allocated += VMM_PAGE_SIZE;
            }

            /* Copy data belonging to this page if within p_filesz */
            uint64_t seg_offset_start = (page_va > phdr->p_vaddr) ? (page_va - phdr->p_vaddr) : 0;
            uint64_t page_offset = (phdr->p_vaddr > page_va) ? (phdr->p_vaddr - page_va) : 0;

            if (seg_offset_start < phdr->p_filesz) {
                uint64_t bytes_to_copy = phdr->p_filesz - seg_offset_start;
                uint64_t space_in_page = VMM_PAGE_SIZE - page_offset;
                if (bytes_to_copy > space_in_page) {
                    bytes_to_copy = space_in_page;
                }

                uint8_t *dst = (uint8_t *)vmm_phys_to_virt(phys) + page_offset;
                const uint8_t *src = base + phdr->p_offset + seg_offset_start;
                memcpy(dst, src, (size_t)bytes_to_copy);
            }
        }
    }

    /* Allocate user stack */
    uint64_t stack_base = USER_STACK_TOP - (USER_STACK_PAGES * VMM_PAGE_SIZE);
    for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        uint64_t stack_va = stack_base + (i * VMM_PAGE_SIZE);
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            KLOG_ERR("elf", "Out of physical memory allocating user stack");
            return ELF_ERR_NO_MEMORY;
        }

        memset(vmm_phys_to_virt(phys), 0, VMM_PAGE_SIZE);

        uint64_t stack_flags = VMM_FLAG_PRESENT | VMM_FLAG_USER | VMM_FLAG_WRITABLE | VMM_FLAG_NX;
        enum vmm_status vs = vmm_map_page(proc->pml4_virt, stack_va, phys, stack_flags);
        if (vs != VMM_OK) {
            KLOG_ERR("elf", "Failed to map user stack page 0x%016llx: %d", (unsigned long long)stack_va, vs);
            return ELF_ERR_MAP_FAILED;
        }
        proc->memory_bytes_allocated += VMM_PAGE_SIZE;
    }

    if (out_entry) {
        *out_entry = ehdr->e_entry;
    }
    if (out_stack) {
        *out_stack = USER_STACK_TOP;
    }

    KLOG_INFO("elf", "Loaded ELF binary for process '%s' (entry: 0x%016llx, stack: 0x%016llx)",
              proc->name, (unsigned long long)ehdr->e_entry, (unsigned long long)USER_STACK_TOP);
    return ELF_OK;
}

const char *elf_status_name(enum elf_load_status status)
{
    switch (status) {
    case ELF_OK: return "ELF_OK";
    case ELF_ERR_INVALID_HEADER: return "ELF_ERR_INVALID_HEADER";
    case ELF_ERR_NOT_64BIT: return "ELF_ERR_NOT_64BIT";
    case ELF_ERR_NOT_X86_64: return "ELF_ERR_NOT_X86_64";
    case ELF_ERR_NOT_EXECUTABLE: return "ELF_ERR_NOT_EXECUTABLE";
    case ELF_ERR_BAD_SEGMENT: return "ELF_ERR_BAD_SEGMENT";
    case ELF_ERR_NO_MEMORY: return "ELF_ERR_NO_MEMORY";
    case ELF_ERR_MAP_FAILED: return "ELF_ERR_MAP_FAILED";
    default: return "ELF_ERR_UNKNOWN";
    }
}
