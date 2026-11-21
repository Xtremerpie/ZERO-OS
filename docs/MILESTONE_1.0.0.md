# Milestone 1.0.0 — Core Foundation: Engineering Completion Report

**Official Version:** 1.0.0  
**Codename:** Core Foundation (Core Awakening)  
**Status:** IMPLEMENTED, TESTED, AND VALIDATED  
**Architecture:** x86_64 Higher-Half Kernel  
**Firmware:** UEFI / Limine 11.4.1 (Base Revision 6)  

---

## 1. Delivered Subsystems & Features

### A. Physical Memory Manager (PMM)
- **Source:** `kernel/memory/memmap.c`, `kernel/memory/memmap.h`, `kernel/memory/pmm.c`, `kernel/memory/pmm.h`
- Normalized physical memory map engine with sorting, priority classification, and overlap failsafe.
- 4 KiB granular bitmap frame allocator.
- Strict sub-1 MiB exclusion (first 1 MiB permanently reserved for legacy/firmware/SMP safety).
- Kernel image and GOP framebuffer region protection.
- 20 unit and stress self-tests validated across a guest matrix (128 MB to 4 GB).

### B. Virtual Memory Manager (VMM)
- **Source:** `kernel/memory/vmm.c`, `kernel/memory/vmm.h`
- 4-level x86_64 paging architecture (PML4, PDPT, PD, PT).
- 48-bit canonical virtual address validation (`vmm_is_canonical`).
- Dynamic 4 KiB page mapping (`vmm_map_page`, `vmm_map_pages`) with page table frame allocation through the PMM.
- Page unmapping and TLB invalidation (`vmm_unmap_page`, `vmm_invlpg`).
- $W \oplus X$ (Write XOR Execute) memory permission enforcement:
  - Kernel `.text`: Read-Only + Executable (`PRESENT`, `NX=0`, `WRITABLE=0`).
  - Kernel `.rodata`: Read-Only + Non-Executable (`PRESENT`, `NX=1`, `WRITABLE=0`).
  - Kernel `.data` and `.bss`: Read-Write + Non-Executable (`PRESENT`, `NX=1`, `WRITABLE=1`).
- Higher-Half Direct Map (HHDM) retention at `0xffff800000000000` covering physical memory.
- GOP linear framebuffer mapped at its virtual address with cache-disable (`VMM_FLAG_NOCACHE`).
- Address space cloning and isolation (`vmm_create_address_space`, `vmm_destroy_address_space`).
- Kernel `CR3` switching to authoritative ZERO OS page tables (`vmm_switch_pml4`).

### C. Kernel Heap Allocator
- **Source:** `kernel/memory/heap.c`, `kernel/memory/heap.h`
- Hybrid allocator:
  - 8 segregated size classes for small objects: 16B, 32B, 64B, 128B, 256B, 512B, 1024B, 2048B.
  - Direct contiguous page allocation for large objects ($> 2048$ bytes) backed by PMM/VMM pages.
- Guaranteed 16-byte alignment on all returned pointers.
- Security and integrity defenses:
  - 64-bit magic header canaries (`0x5A45524F48454150ULL`).
  - 64-bit magic tail canaries (`0x43414E4152593437ULL`) placed immediately after requested size.
  - Double-free detection and trapping (`0x465245455A45524FULL`).
  - Header corruption and buffer overflow detection.
- Core APIs: `kmalloc`, `kfree`, `kcalloc`, `krealloc`.
- Heap verification and leak invariant checking.

### D. Concurrency & Spinlocks
- **Source:** `kernel/include/zero/spinlock.h`, `kernel/lib/spinlock.c`
- Atomic test-and-set spinlocks utilizing compiler atomics and the x86 `pause` instruction.
- Non-blocking `spinlock_try_acquire()`.
- IRQ-safe critical sections: `spin_lock_irqsave(lock, &flags)` and `spin_unlock_irqrestore(lock, flags)`.
- Deadlock timeout detection.

### E. Interrupts, Local APIC & APIC Timer
- **Source:** `kernel/include/zero/apic.h`, `kernel/arch/x86_64/apic.c`, `kernel/include/zero/timer.h`, `kernel/arch/x86_64/timer.c`, `kernel/interrupts/idt.c`, `kernel/arch/x86_64/isr.asm`
- Discovery of Local APIC via CPUID (leaf 1 EDX bit 9) and APIC Base MSR (`0x1B`).
- Legacy 8259 PIC properly masked on ports `0x21` and `0xA1`.
- Local APIC software enabled with Spurious Interrupt Vector `0xFF`.
- IDT expanded from 32 exception gates to 48 gates (vectors 0-31 exceptions, vectors 32-47 hardware IRQs).
- Dynamic IRQ handler registration via `idt_register_handler()`.
- Periodic APIC Timer configured at 100 Hz on vector `0x20` (IRQ0).
- Uptime millisecond tracking and tick counting.
- End-Of-Interrupt (`apic_eoi()`) signaling.

---

## 2. Acceptance Checklist

| Requirement | Implementation State | Verification Method |
|---|---|---|
| Official Version Policy (Exactly 10 releases) | **Passed** | Automated test `check_versions.py` (Exit code 0) |
| Physical Memory Manager (PMM) | **Passed** | 20/20 PMM selftests passing across 128M - 4G guests |
| Virtual Memory Manager (VMM) | **Passed** | 4-level paging, canonical check, translation, W^X |
| Kernel Heap Allocator | **Passed** | 8 size classes, large pages, canaries, realloc, leak-free |
| Concurrency & Spinlocks | **Passed** | Atomic acquire/release, try-acquire, IRQ-save |
| Local APIC & Timer | **Passed** | Legacy PIC masked, APIC enabled, 100 Hz timer tick |
| Build Quality Gates | **Passed** | `-Wall -Wextra -Werror` clean, zero compiler warnings |

---

## 3. Next Official Milestone

**Milestone 2.0.0 — System Architecture**
- Process Control Blocks (PCB) with private address spaces.
- Thread Control Blocks (TCB) and preemptive multitasking.
- Context switching assembly routine (`switch_to`).
- Round-robin / priority-based scheduler.
- Inter-Process Communication (IPC).
- Virtual File System (VFS) and Ramfs.
- ELF64 executable loader and Ring 3 user mode isolation.
