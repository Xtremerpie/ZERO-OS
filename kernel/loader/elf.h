/* ZERO KERNEL - ELF64 Executable Loader Interface (Version 2.0.0 System Architecture).
 *
 * Defines 64-bit ELF data structures, header validation, segment loading into
 * process virtual address spaces with proper W^X permissions, and user stack allocation.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "process/process.h"

#define EI_NIDENT       16

#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2

#define ELFDATANONE     0
#define ELFDATA2LSB     1 /* Little-endian */
#define ELFDATA2MSB     2 /* Big-endian */

#define EV_CURRENT      1

#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4

#define EM_NONE         0
#define EM_386          3
#define EM_X86_64       62

#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_TLS          7

#define PF_X            0x1
#define PF_W            0x2
#define PF_R            0x4

#define USER_STACK_TOP   0x00007FFFF0000000ull
#define USER_STACK_PAGES 4ull /* 16 KiB default user stack */

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

enum elf_load_status {
    ELF_OK = 0,
    ELF_ERR_INVALID_HEADER,
    ELF_ERR_NOT_64BIT,
    ELF_ERR_NOT_X86_64,
    ELF_ERR_NOT_EXECUTABLE,
    ELF_ERR_BAD_SEGMENT,
    ELF_ERR_NO_MEMORY,
    ELF_ERR_MAP_FAILED
};

/* Validate an in-memory ELF image without modifying state. */
enum elf_load_status elf_validate(const void *image, size_t size);

/* Load an ELF image into a process address space and configure entry point and user stack. */
enum elf_load_status elf_load(process_t *proc, const void *image, size_t size, uint64_t *out_entry, uint64_t *out_stack);

/* Return human-readable error name. */
const char *elf_status_name(enum elf_load_status status);
