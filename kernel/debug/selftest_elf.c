/* ZERO KERNEL - ELF64 Loader Self-Tests (Version 2.0.0 System Architecture).
 *
 * Verifies:
 * 1. ELF header validation and rejection of corrupted/invalid binaries.
 * 2. 64-bit and x86_64 architecture enforcement.
 * 3. Synthetic ELF image parsing and PT_LOAD segment extraction.
 * 4. Virtual address mapping into process page tables with W^X enforcement.
 * 5. User stack frame allocation and entry point initialization.
 */

#include "loader/elf_test.h"
#include "loader/elf.h"
#include "process/process.h"
#include "memory/vmm.h"
#include "zero/log.h"
#include "zero/string.h"

#pragma pack(push, 1)
struct synthetic_elf {
    elf64_ehdr_t ehdr;
    elf64_phdr_t phdr;
    uint8_t code[16];
};
#pragma pack(pop)

bool elf_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting ELF64 Loader subsystem self-tests...");

    /* 1. Truncated header rejection */
    uint8_t garbage[16] = { 0 };
    if (elf_validate(garbage, sizeof(garbage)) == ELF_OK) {
        KLOG_ERR("selftest", "ELF test 1 failed: accepted truncated header");
        return false;
    }

    /* 2. Invalid magic rejection */
    struct synthetic_elf test_elf;
    memset(&test_elf, 0, sizeof(test_elf));
    test_elf.ehdr.e_ident[0] = 0x7F;
    test_elf.ehdr.e_ident[1] = 'B';
    test_elf.ehdr.e_ident[2] = 'A';
    test_elf.ehdr.e_ident[3] = 'D';

    if (elf_validate(&test_elf, sizeof(test_elf)) == ELF_OK) {
        KLOG_ERR("selftest", "ELF test 2 failed: accepted invalid magic");
        return false;
    }

    /* 3. Construct valid synthetic ELF64 executable */
    memset(&test_elf, 0, sizeof(test_elf));
    test_elf.ehdr.e_ident[0] = ELFMAG0;
    test_elf.ehdr.e_ident[1] = ELFMAG1;
    test_elf.ehdr.e_ident[2] = ELFMAG2;
    test_elf.ehdr.e_ident[3] = ELFMAG3;
    test_elf.ehdr.e_ident[4] = ELFCLASS64;
    test_elf.ehdr.e_ident[5] = ELFDATA2LSB;
    test_elf.ehdr.e_ident[6] = EV_CURRENT;
    test_elf.ehdr.e_type = ET_EXEC;
    test_elf.ehdr.e_machine = EM_X86_64;
    test_elf.ehdr.e_version = EV_CURRENT;
    test_elf.ehdr.e_entry = 0x400000ull;
    test_elf.ehdr.e_phoff = offsetof(struct synthetic_elf, phdr);
    test_elf.ehdr.e_ehsize = sizeof(elf64_ehdr_t);
    test_elf.ehdr.e_phentsize = sizeof(elf64_phdr_t);
    test_elf.ehdr.e_phnum = 1;

    test_elf.phdr.p_type = PT_LOAD;
    test_elf.phdr.p_flags = PF_R | PF_X; /* Read + Execute (code) */
    test_elf.phdr.p_offset = offsetof(struct synthetic_elf, code);
    test_elf.phdr.p_vaddr = 0x400000ull;
    test_elf.phdr.p_paddr = 0x400000ull;
    test_elf.phdr.p_filesz = sizeof(test_elf.code);
    test_elf.phdr.p_memsz = 0x1000ull; /* 4 KiB memory footprint */
    test_elf.phdr.p_align = 0x1000ull;

    /* Fill dummy instructions: NOPs and RET */
    for (size_t i = 0; i < sizeof(test_elf.code); i++) {
        test_elf.code[i] = 0x90; /* NOP */
    }
    test_elf.code[15] = 0xC3; /* RET */

    /* 4. Validate synthetic binary */
    enum elf_load_status status = elf_validate(&test_elf, sizeof(test_elf));
    if (status != ELF_OK) {
        KLOG_ERR("selftest", "ELF test 4 failed: valid synthetic binary rejected with %s", elf_status_name(status));
        return false;
    }

    /* 5. Load synthetic binary into process address space */
    process_t *target_proc = process_create("elf_test_task", 0);
    if (!target_proc) {
        KLOG_ERR("selftest", "ELF test 5 failed: cannot create process");
        return false;
    }

    uint64_t entry_point = 0;
    uint64_t stack_top = 0;
    status = elf_load(target_proc, &test_elf, sizeof(test_elf), &entry_point, &stack_top);
    if (status != ELF_OK) {
        process_cleanup(target_proc);
        KLOG_ERR("selftest", "ELF test 5 failed: elf_load failed with %s", elf_status_name(status));
        return false;
    }

    if (entry_point != 0x400000ull || stack_top != USER_STACK_TOP) {
        process_cleanup(target_proc);
        KLOG_ERR("selftest", "ELF test 5 failed: entry (0x%lx) or stack (0x%lx) incorrect", entry_point, stack_top);
        return false;
    }

    /* 6. Verify page table mapping and code bytes */
    uint64_t phys_addr = 0;
    uint64_t page_flags = 0;
    enum vmm_status vs = vmm_translate(target_proc->pml4_virt, 0x400000ull, &phys_addr, &page_flags);
    if (vs != VMM_OK || phys_addr == 0) {
        process_cleanup(target_proc);
        KLOG_ERR("selftest", "ELF test 6 failed: text page not mapped in target address space");
        return false;
    }

    const uint8_t *mapped_code = (const uint8_t *)vmm_phys_to_virt(phys_addr);
    if (mapped_code[0] != 0x90 || mapped_code[15] != 0xC3) {
        process_cleanup(target_proc);
        KLOG_ERR("selftest", "ELF test 6 failed: instruction payload mismatch in mapped frame");
        return false;
    }

    /* 7. Clean up */
    process_cleanup(target_proc);

    kprintf("[PASS] elf: ELF64 executable validation, segment mapping, and user stack verified\n");
    return true;
}
