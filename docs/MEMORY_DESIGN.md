# Memory Management Design

**Status:** the physical memory manager section is IMPLEMENTED and TESTED (see `docs/MEMORY_MANAGEMENT.md`, `docs/PMM_DESIGN.md`).
Virtual memory and the kernel heap below are **DESIGNED ONLY / NOT IMPLEMENTED**.

## Inputs (available now)

Limine provides typed regions (usable, reserved, ACPI, bootloader-reclaimable,
kernel+modules, framebuffer, reserved-mapped) and an HHDM offset. Observed in QEMU: 512 MB guest →
~461 MiB usable; 4 GB guest → 4045 MiB usable. Reserved ranges include the 64-bit PCI hole and are
large; they must never be touched.

## Physical memory manager (1.0.0)

- Bitmap allocator over usable regions at 4 KiB granularity (chosen over a buddy allocator for
  size and simplicity at 4 GB; revisit if fragmentation measurements justify it).
- Bootloader-reclaimable memory is released only after the kernel stops using Limine-provided tables.
- Bitmap lives in the first usable region large enough, addressed through the HHDM.
- Failure paths: allocation returns NULL/error; no silent panics outside early boot.

## Virtual memory (1.0.0)

- Own 4-level page tables replacing Limine's. Kernel text/rodata/data mapped with W^X permissions; NX enabled (observed `EFER.NXE` set).
- HHDM kept for physical access; guard pages around kernel stacks.
- Per-process address spaces from 2.0.0; user/kernel split at the canonical hole.

## Kernel heap (1.0.0)

Slab-style size classes on top of the PMM; allocation statistics exposed for the benchmarks
required by the directive (allocation performance, kernel memory usage).

## Targets (aspirational, not measured)

Idle memory < 256 MB on the 4 GB target. Not measurable until a PMM and services exist.
