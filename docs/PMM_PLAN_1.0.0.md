> **SUPERSEDED.** This is the pre-implementation plan. The implemented design, including deviations (invalid frees are rejected and counted, not a panic), is `docs/PMM_DESIGN.md`.

# Implementation Plan — Physical Memory Manager (first Version 1.0.0 subsystem)

**Status: PLAN ONLY. Nothing in this document is implemented. Implementation starts only after the project owner approves
(see "Decisions needed" at the end).**

Identity: the official version stays **0.0.0** in the manifest and `version.h` while this subsystem is developed (`VERSION_POLICY.md`):
work in progress is identified by git hash. `configs/versions.json` is not touched by this plan.

## 1. Goal and non-goals

**Goal:** a correct, small, tested allocator of 4 KiB physical page frames, built from the bootloader's memory map, that
every later 1.0.0 subsystem (page tables, kernel heap, thread stacks) will depend on.

**Not in this step:** virtual memory/page tables, kernel heap, reclaiming bootloader memory, zeroing policy, DMA zones, huge pages,
NUMA, SMP locking, hot-plug, swapping. Those are later 1.0.0 steps (see `ROADMAP.md`); stubs for them will **not** be added (Rule 5).

## 2. Facts the design rests on (all observed in this repository's QEMU runs)

From the real Limine memory map (a diagnostic dump, not committed):

- Regions are **sorted ascending, non-overlapping, 4 KiB-aligned**. Types seen: usable, bootloader-reclaimable, acpi-nvs,
  acpi-reclaimable, reserved, reserved-mapped, framebuffer, kernel+modules.
- 256 MB guest: **7** usable ranges, e.g. `0x1000+0x9f000`, `0x100000+0x700000`, `0x1780000+0xa3f5000`, … (page 0 is *bootloader-reclaimable*).
- 4 GB guest: usable ranges include `0x1780000+0x7a3f5000` and **`0x100000000+0x80000000`** (RAM above 4 GiB), plus a reserved
  12 GiB PCI window at `0xfd00000000`. The highest *usable* end is **6 GiB**, the highest address of any region is ≫ 4 GiB.
  ⇒ the bitmap must span to the highest **usable** end, never to the highest region end.
- The kernel image is its own region (`kernel+modules`, 12 pages), the framebuffer is its own region — both are *not* usable, so excluded automatically.
- Limine's direct map (HHDM, `0xffff800000000000`) **covers every usable range, including the one above 4 GiB**
  (checked with QEMU's page-table dump); legacy `0xA0000–0xFFFFF` is unmapped. The PMM may therefore touch any usable page through the HHDM.
- Limine keeps its own structures (including the memory-map response array and the boot page tables) in *bootloader-reclaimable*
  memory, so that memory must **not** be freed yet.
- Minimum guest RAM that boots at all: 128 MB (the firmware stage fails at 96 MB).

## 3. Design decisions (with alternatives)

| # | Decision | Choice | Rejected alternatives and why |
|---|----------|--------|-------------------------------|
| D1 | Data structure | **Bitmap**, 1 bit per 4 KiB frame (1 = used) | *Buddy*: more code, per-page metadata, needed only if fragmentation measurements justify it. *Free-list stack*: cannot allocate contiguous runs and cannot detect double-free cheaply |
| D2 | Low memory | Never hand out frames below **1 MiB** | Using it adds risk (legacy/firmware/real-mode uses, SMP trampolines later) for 640 KiB of benefit |
| D3 | Bootloader-reclaimable | Counted and reported but **not freed** in this step | Freeing now would destroy structures the kernel still uses (stack, page tables, memmap array). A later step reclaims after the VMM switches to ZERO's own tables |
| D4 | Error policy | Out of memory → return `0` (physical address 0 is never valid). Invalid free (unaligned, outside managed ranges, in the bitmap, in a reserved page) or double free → **`kpanic`** with a precise message | Returning an error code from `free` invites ignored errors; these are kernel bugs, not runtime conditions |
| D5 | Concurrency | None. Single CPU, interrupts masked; stated as a documented precondition. Locking is added in the step that introduces interrupts/SMP | A lock now would be an untestable placeholder |
| D6 | Zeroing | The PMM does **not** zero pages; callers do (`memset` via `pmm_phys_to_virt`) | Hidden zeroing costs time on every alloc and is not always needed |
| D7 | Interface independence | `pmm_init` takes a plain array of `{base, length}` usable ranges — **no Limine types** inside the PMM | Coupling to the bootloader would make the PMM untestable on its own and block replacing Limine later |
| D8 | Range bookkeeping | Copy the usable ranges into a fixed table (256 entries, 4 KiB) at init; this is what validates every free | A second "managed" bitmap would double the memory; a table is tiny and also lets Limine's array be reclaimed later. >256 usable ranges → panic at init, loudly |
| D9 | Bitmap placement | Carve from the **first usable range ≥ 1 MiB that can hold it**, at its start; those frames are permanently marked used | Fixed address (not portable); static array (cannot size for 4+ GiB and wastes BSS on small machines) |

## 4. Public interface (`kernel/include/zero/pmm.h`)

```c
#define PMM_PAGE_SIZE 4096u

struct pmm_range { uint64_t base; uint64_t length; };   /* bytes; 4 KiB aligned */

struct pmm_stats {
    uint64_t managed_pages;    /* usable frames >= 1 MiB, excluding the bitmap itself */
    uint64_t free_pages;
    uint64_t used_pages;       /* managed_pages - free_pages */
    uint64_t bitmap_bytes;
    uint64_t highest_end;      /* physical end of the highest usable range */
    uint64_t allocs, frees;    /* lifetime counters */
};

/* Must be called exactly once, before any other pmm_* call. Panics on a malformed map
 * (unsorted, overlapping, unaligned, >256 ranges, no room for the bitmap). */
void     pmm_init(const struct pmm_range *usable, size_t count, uint64_t hhdm_offset);

uint64_t pmm_alloc_page(void);                 /* physical address, or 0 if out of memory */
uint64_t pmm_alloc_pages(size_t count);        /* `count` physically contiguous frames, or 0 */
void     pmm_free_page(uint64_t phys);         /* panics on invalid/double free */
void     pmm_free_pages(uint64_t phys, size_t count);
void     pmm_get_stats(struct pmm_stats *out);

static inline void *pmm_phys_to_virt(uint64_t phys);   /* HHDM */
```
Invariant checked by every test: `free_pages + used_pages == managed_pages`.

## 5. Algorithms

- **init:** validate ranges (sorted, disjoint, aligned) → copy to the table, clipping everything below 1 MiB → compute
  `highest_end` → `bitmap_bytes = ceil(highest_pfn/8)` rounded up to whole frames → find the placement range (D9) →
  fill the bitmap with 1s → clear bits for every managed frame → set bits for the bitmap's own frames → set counters.
- **alloc_page:** from a rotating hint, scan the bitmap 64 bits at a time (`__builtin_ctzll` on the inverted word), set the bit, update counters and hint. Worst case O(span/64), typically O(1).
- **alloc_pages(n):** first-fit scan for `n` consecutive clear bits that lie inside **one** usable range (a free run must never bridge a hole); set them; `n == 0` returns 0.
- **free:** reject if unaligned, not inside a table range, inside the bitmap's frames, or the bit is already clear (double free) — each case panics with a distinct message — then clear the bit and update counters/hint.

Memory cost: 1 bit per frame ⇒ 32 KiB per GiB of address span. The 4 GB guest spans 6 GiB ⇒ **192 KiB** (0.005 % of 4 GiB). Static kernel growth: ≈ 4 KiB table + code (< 8 KiB total budget).

## 6. Files

| File | Change |
|------|--------|
| `kernel/include/zero/pmm.h` | **new** — interface above |
| `kernel/memory/pmm.c` | **new** — implementation (~300 lines) |
| `kernel/debug/selftest_pmm.c` | **new** — PMM selftests (keeps test code out of the PMM) |
| `kernel/debug/selftest.c`, `selftest.h` | dispatch the new selftest names; `selftest_run` receives the Limine memory-map pointer so tests can recompute expected values **independently** of the PMM |
| `kernel/main.c` | after `report_memory_map()`: convert usable regions → `pmm_range[]`, call `pmm_init`, log stats |
| `tests/run_tests.py` | new cases (§7); per-case guest size |
| `docs/MEMORY_DESIGN.md`, `ARCHITECTURE.md`, `TESTING.md`, `PERFORMANCE.md`, `CHANGELOG.md` | moved from "designed" to "implemented" **only after** the tests pass |

No Makefile change is needed (sources are discovered); `make analyze` and `make ubsan` automatically cover the new files.

## 7. Test plan (all run in QEMU; expected outcome stated first)

| Selftest (`zero.selftest=`) | Guest RAM | Expected | What it proves |
|---|---|---|---|
| `pmm_init` | 128M, 256M, 512M, 4G | boot completes; `managed_pages` equals an **independent** recount from the Limine map (usable ≥ 1 MiB minus bitmap); invariant holds | init arithmetic, bitmap placement, sub-1 MiB exclusion, range above 4 GiB handled |
| `pmm_alloc` | 512M, 4G | 4096 pages: all unique, 4 KiB aligned, ≥ 1 MiB, each inside a usable range, none overlapping the kernel/framebuffer/bitmap regions | allocation correctness and isolation |
| `pmm_write` | 512M | unique 64-bit pattern written to every one of the 4096 pages via the HHDM, then all read back intact | no two allocations alias; pages really writable |
| `pmm_free` | 512M | free every second page → stats consistent → reallocate: the freed frames are reused (set equality) → free all → `free_pages` back to the starting value | free/reuse and counters |
| `pmm_contig` | 512M | `alloc_pages(16)` is contiguous and fresh; a request larger than any single range returns 0 without corrupting state; free restores stats | contiguous path, range-bridging guard |
| `pmm_exhaust` | **128M** | allocate until `0`: allocated count == `managed_pages` exactly; next call returns 0 (no panic); free everything → `free_pages == managed_pages` | out-of-memory behaviour and no leak |
| `pmm_badfree_unaligned`, `_range`, `_reserved`, `_bitmap`, `pmm_doublefree` | 256M | each ends in a panic with its own message (exit 35) | every invalid-free class is caught |
| `pmm_bench` | 512M | logs cycles per alloc/free (`rdtsc`, 100 000 ops); no pass/fail threshold | baseline for `docs/PERFORMANCE.md` (TSC under TCG is virtual — recorded as such) |

**Quality gates** (must stay green): `make` with `-Werror`, `make analyze`, the full suite on the normal build, the full suite on the UBSan build.
**Mutation checks** (done during implementation, results reported): remove the double-free check → `pmm_doublefree` must fail; shift the 1 MiB limit → `pmm_init`/`pmm_alloc` must fail;
let a free run bridge a hole → `pmm_contig` must fail. A test that cannot fail is not accepted.

## 8. Work breakdown (each step is a git checkpoint)

1. Header + range validation + table (compile-only, `-Werror`).
2. `pmm_init` + bitmap placement + stats; `pmm_init` selftest.
3. `alloc_page`/`free_page`; `pmm_alloc`, `pmm_write`, `pmm_free` selftests.
4. `alloc_pages`/`free_pages`; `pmm_contig`, `pmm_exhaust`.
5. Invalid-free panics; bad-free selftests.
6. Harness cases + guest-size matrix; run normal, 256M, UBSan suites; analyzer.
7. Mutation checks; benchmark; documentation updates; report.

## 9. Acceptance criteria for this subsystem

1. All §7 tests pass on 128M/256M/512M/4G as specified, normal and UBSan builds; analyzer clean; `-Werror` clean.
2. Each mutation in §7 makes its test fail.
3. `managed_pages` equals the independent recount in every guest size; invariant `free + used == managed` holds after every test phase.
4. Static kernel growth < 8 KiB; bitmap cost reported per guest size.
5. Docs updated to match code; "designed → implemented" changes only for what is tested. No new official version; manifest unchanged.
6. The existing 16 boot/exception tests still pass unchanged.

## 10. Risks

| Risk | Mitigation |
|------|-----------|
| A real machine's memory map differs (more/smaller ranges, ranges < 1 MiB only) | table limit is a loud panic, not silent truncation; sub-1 MiB clipping handled; behaviour on real hardware is **unverified** until 3.0.0 |
| Bitmap needs a contiguous usable range | observed ranges are hundreds of MiB; if none fits, `pmm_init` panics with a clear message |
| Reading/writing a usable page that the HHDM does not map | verified for all four observed ranges (incl. > 4 GiB); `pmm_write` re-checks at runtime on every guest size |
| Exhaustion/stress tests slow under emulation | exhaustion runs only in the 128 MB guest (~30 k pages); 4 GB guest runs only O(1000) operations |
| Tests that share code with the PMM would hide bugs | expected values are recomputed from the Limine map, not from PMM state |
| Scope creep into VMM/heap | explicitly excluded (§1); reviewed at step 7 |

## 11. Decisions needed from the project owner (reply "approved", or change any default)

1. **Bitmap allocator** (D1) — default: approved.
2. **Exclude all memory below 1 MiB** (D2) — default: yes.
3. **Do not reclaim bootloader memory in this step** (D3) — default: yes.
4. **Panic on invalid/double free; return 0 on out-of-memory** (D4) — default: yes.
5. **Plain `pmm_range` interface, no Limine types inside the PMM** (D7) — default: yes.
6. **Guest-size matrix 128M / 256M / 512M / 4G** (§7) — default: yes.

No code for the PMM has been written. On approval I will start at step 1 of §8.
