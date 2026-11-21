/* ZERO KERNEL - boot facts handed to subsystems that must cross-check the bootloader data
 * independently (used by the PMM selftests). Pointers are bootloader-owned and read-only. */
#pragma once

#include <stdint.h>

struct limine_memmap_response;

struct boot_info {
    const struct limine_memmap_response *memmap; /* raw bootloader map (may be NULL) */
    uint64_t hhdm_offset;                        /* virtual = physical + hhdm_offset */
    uint64_t kernel_phys_base;                   /* physical load address of the kernel image */
    uint64_t kernel_size;                        /* bytes, __kernel_start .. __kernel_end */
    uint64_t fb_phys;                            /* framebuffer physical base, 0 if none */
    uint64_t fb_size;                            /* framebuffer bytes, 0 if none */
};
