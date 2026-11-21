# Memory Management (Version 1.0.0 — physical memory only)

Status legend used in this file: **IMPLEMENTED** (code exists) · **TESTED** (a test in `make test` exercises it, results in
`docs/VALIDATION_REPORT_1.0.0.md`) · **DESIGNED ONLY** · **NOT IMPLEMENTED** · **KNOWN LIMITATION**.

## What exists

| Component | Files | Status |
|-----------|-------|--------|
| Memory-map engine (normalize, validate, classify) | `kernel/memory/memmap.[ch]` | IMPLEMENTED, TESTED |
| Limine adapter (the only Limine-aware memory code) | `kernel/memory/memmap_limine.c` | IMPLEMENTED, TESTED (on real Limine maps in QEMU) |
| Bitmap physical memory manager | `kernel/memory/pmm.[ch]` | IMPLEMENTED, TESTED |
| Checked arithmetic helpers | `kernel/include/zero/checked.h` | IMPLEMENTED, TESTED (through the engine) |
| PMM selftests and benchmark | `kernel/debug/selftest_pmm.c`, `kernel/memory/pmm_test.h` | IMPLEMENTED |
| Virtual memory manager / own page tables | — | **NOT IMPLEMENTED** (later in 1.0.0) |
| Kernel heap | — | **NOT IMPLEMENTED** |
| Reclaiming bootloader / ACPI-reclaimable memory | — | **NOT IMPLEMENTED** (deliberate, see below) |
| Page zeroing, NUMA, huge-page awareness, memory hot-plug | — | **NOT IMPLEMENTED** |

## Boot order (IMPLEMENTED, TESTED)

CPU report → GDT/TSS → IDT → diagnostics (raw bootloader map + HHDM) → **memory-map validation → PMM init →
PMM boot allocation check** → selftests (if requested) → boot complete. If validation or PMM init fails,
the kernel **panics and halts** (QEMU exit 35 under `zero.test_exit=1`); it never continues with a half-built allocator
(TESTED by `memmap_overlap_failsafe`).

## Memory-map classification rules (IMPLEMENTED, TESTED)

Input: bootloader entries → `struct mem_raw_entry {base, length, type}`.

1. A null map, null entry, zero or more than 128 entries is rejected. Zero-length entries are ignored.
2. `base + length` must not wrap and must not exceed 2^52; the type must be known. Otherwise the whole map is rejected.
3. Entries are copied and insertion-sorted by base. Any overlap between two entries rejects the whole map.
4. Alignment to 4096 bytes: **usable** regions shrink *inward* (a partial page is never handed out);
   every other type expands *outward* (a partial page is never treated as free).
5. A page claimed by several regions takes the highest-priority type:
   `KERNEL > FRAMEBUFFER > ACPI_NVS > RESERVED = OTHER > BAD > ACPI_RECLAIMABLE > BOOTLOADER_RECLAIMABLE > USABLE`.
6. Adjacent regions of the same type are merged. Address space no entry describes is a **hole**: it is not in the table and
   is never allocatable.
7. After normalization there must be at least one usable page.

| Limine type | Internal type | Allocatable? |
|-------------|---------------|--------------|
| USABLE | `MEM_USABLE` | **yes**, only at/above 1 MiB and not the bitmap itself |
| RESERVED | `MEM_RESERVED` | no |
| ACPI_RECLAIMABLE | `MEM_ACPI_RECLAIMABLE` | no (not reclaimed yet) |
| ACPI_NVS | `MEM_ACPI_NVS` | never |
| BAD_MEMORY | `MEM_BAD` | never |
| BOOTLOADER_RECLAIMABLE | `MEM_BOOTLOADER_RECLAIMABLE` | no (not reclaimed yet) |
| EXECUTABLE_AND_MODULES | `MEM_KERNEL` | never |
| FRAMEBUFFER | `MEM_FRAMEBUFFER` | never |
| RESERVED_MAPPED, unknown | `MEM_OTHER` | never |

Policy: **frames below 1 MiB are never allocated** even if the bootloader calls them usable (legacy/firmware-owned area).
KNOWN LIMITATION: kernel and framebuffer protection rests on the bootloader classifying them correctly; tests cross-check
this against the raw bootloader map and the kernel's own linker symbols, but the PMM itself has no other source of truth.

## API (IMPLEMENTED, TESTED) — `kernel/memory/pmm.h`

Physical addresses are `uint64_t`, never pointers. Access physical memory only via the HHDM (`virtual = physical + offset`).

| Function | Behaviour |
|----------|-----------|
| `pmm_init(map, hhdm)` | one-time; `PMM_ERR_ALREADY_INITIALIZED` on repeat, state untouched |
| `pmm_is_initialized()` | bool |
| `pmm_alloc_page()` | physical address, **0 on failure** (frame 0 is never allocatable) |
| `pmm_alloc_pages(n)` | `n` physically contiguous frames, first-fit; 0 on failure |
| `pmm_free_page(a)`, `pmm_free_pages(a, n)` | returns `pmm_status`; **rejects** (logs nothing, counts) unaligned, out-of-range, wrapped, protected, and already-free frames; a multi-frame free is **atomic** |
| `pmm_get_stats(&s)` | see below |

Status codes: `PMM_OK, NOT_INITIALIZED, ALREADY_INITIALIZED, BAD_MAP, NO_STORAGE, UNALIGNED, RANGE, PROTECTED, NOT_ALLOCATED, COUNT, NO_MEMORY`.
An instance API (`pmm_instance_*`, `struct pmm`) exists so tests can run on synthetic maps; the kernel uses one global instance.

## Statistics (IMPLEMENTED, TESTED)

total / usable / reserved / allocatable / free / allocated memory (bytes) and frames; allocation calls and failures; free calls,
invalid frees and double frees; peak allocated frames; bitmap bytes and tracked frames.
"Total memory" is the sum of everything the bootloader *described*, including large reserved ranges such as the 64-bit PCI hole
(about 12 GiB in QEMU); it is **not** installed RAM.

## Log format (IMPLEMENTED)

`[INFO ] memory: memory map validated: N bootloader entries -> M regions`, per-type lines,
`[INFO ] pmm: initialized: F frames tracked, bitmap B bytes`, `total/usable/reserved`, `allocatable/free`,
`[PASS] pmm: allocation test`. All numbers are runtime values. Existing `mem:` diagnostic lines are unchanged.

## Selftest options (kernel command line, IMPLEMENTED)

`zero.selftest=pmm` (20 tests, panics on any failure) · `zero.selftest=pmm_bench` · `zero.selftest=memmap_overlap`
(feeds the PMM a corrupt map; boot must fail safe).

## Synchronization (KNOWN LIMITATION)

No locks. Not thread-safe, not interrupt-safe. Valid in 1.0.0 only because the kernel is single-CPU with interrupts disabled.
A lock (and IRQ masking) must be added before the scheduler, IRQs or SMP can call the PMM.
