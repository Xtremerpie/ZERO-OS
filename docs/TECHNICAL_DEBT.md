# ZERO OS — Technical Debt & Architectural Boundaries

**Milestone:** 1.0.0 Core Foundation  
**Auditor:** Principal OS Architect  
**Status:** Monitored  

---

## 1. Current Accepted Limits in Milestone 1.0.0

1. **Single-Core Focus:**
   - Milestone 1.0.0 runs exclusively on the Bootstrap Processor (BSP).
   - Local APIC initialization and timer ticks are configured for the active CPU.
   - Symmetric Multiprocessing (SMP) brings up Application Processors (APs) via INIT-SIPI-SIPI sequences; this will be implemented in Milestone 2.0.0/3.0.0.
   - Spinlocks are implemented with atomics and IRQ-save semantics, making them ready for multi-core expansion.

2. **Bootloader-Reclaimable Memory:**
   - The kernel currently has its own authoritative PML4 in `CR3`, which means the kernel is decoupled from Limine's page tables.
   - However, the bootloader memory map array and initial frame structures reside in `BOOTLOADER_RECLAIMABLE` memory.
   - A subsequent step in Milestone 2.0.0 will deep-copy all needed boot structures and call `pmm_free_pages()` on the ~45 MiB of bootloader-reclaimable memory to achieve 100% RAM recovery.

3. **Software-Only Framebuffer:**
   - Display output is handled via UEFI GOP linear framebuffer in 32-bpp RGB.
   - Hardware 2D/3D acceleration for Intel HD Graphics 500 (Gen9 LP) requires writing a complete GPU driver, which is deferred to Milestone 9.0.0.
   - For desktop rendering (Milestone 5.0.0), a dirty-rect double-buffered software compositor will be used to ensure sub-millisecond redraws on the Celeron J3355 without GPU dependencies.

4. **User-Space Transition:**
   - Milestone 1.0.0 establishes the kernel memory model, page tables, dynamic heap, and interrupt handling.
   - User mode (Ring 3), TSS user stack switching, and `syscall`/`sysretq` dispatch are the starting focus of Milestone 2.0.0 (System Architecture).

---

## 2. Milestone 2.0.0 Prerequisites (All Satisfied)

- [x] Physical Memory Manager active and validated.
- [x] Virtual Memory Manager active with 4-level paging and $W \oplus X$.
- [x] Authoritative ZERO OS `CR3` loaded.
- [x] Dynamic Kernel Heap (`kmalloc`, `kfree`, `kcalloc`, `krealloc`) operational.
- [x] Atomic spinlocks and IRQ-safe critical sections ready.
- [x] Local APIC and 100 Hz periodic timer operational.
- [x] IDT hardware interrupt dispatch established.
