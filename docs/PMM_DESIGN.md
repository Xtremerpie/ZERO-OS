# PMM Design (Version 1.0.0)

Supersedes `docs/PMM_PLAN_1.0.0.md` (the pre-implementation plan). Differences from that plan are listed at the end.
Labels: IMPLEMENTED · TESTED · KNOWN LIMITATION · NOT IMPLEMENTED.

## Data structure (IMPLEMENTED)

One bit per 4096-byte frame, `1 = used / unavailable`, `0 = free`. The bitmap covers frames `0 .. highest_usable_end/4096`
(not the whole physical address space), so the reserved PCI hole costs nothing. Padding bits in the last word are permanently 1.
Allocation is first-fit. `hint` is the lowest frame that may be free; **invariant: every frame below `hint` is used**
(re-checked by `pmm_instance_verify`, TESTED).

## Bitmap placement (IMPLEMENTED)

The first usable region at/above 1 MiB that can hold the bitmap; at its start, accessed through the HHDM. Those frames are
marked used and **protected** (frees of them are rejected). No heap is needed.

## Algorithms (IMPLEMENTED, TESTED)

- Single/contiguous allocation: `next_clear` / `next_set_lim` scan 64 frames per word using `ctz`. The run check is **bounded**
  to the requested length; an earlier unbounded version made single-page allocation cost grow with guest size (about 600 cycles at
  128 MB to about 53,000 at 4 GB). Found by the benchmark, fixed, guarded by the test `pmm_alloc_cost_independent_of_memory_size`.
- Free: validation order is alignment → count → arithmetic overflow → region coverage (a gap is `RANGE`, a non-usable region is
  `PROTECTED`) → below-1 MiB policy → bitmap storage → "all frames currently allocated" for the *whole* range before any bit changes
  (atomic).
- Fragmentation measurement: a word-wise scan counts free runs and the longest run.

## Cost (measured; units in `docs/VALIDATION_REPORT_1.0.0.md`)

- **Fixed memory overhead:** `struct pmm` global = 3,280 bytes of BSS (it embeds a private copy of the normalized map, 128 regions
  max, 24 bytes each). Normalization uses about 5 KiB of stack temporarily. No large static arrays.
- **Bitmap:** 1 bit per tracked frame = 32 KiB per GiB of address span up to the highest usable end. Measured: 2,976 B (128 MB guest),
  15,264 B (512 MB), 31,648 B (1 GB), 196,608 B (4 GB guest; the span is 6 GiB because of how QEMU places RAM above the 32-bit hole).
- Allocation and free are O(words scanned); free is O(frames freed + regions).

## Interfaces for later work (DESIGNED ONLY unless stated)

A future VMM needs only: `pmm_alloc_page()` for page-table frames, `pmm_free_page()`, `pmm_hhdm()` (IMPLEMENTED) and, later, a
way to report which frames are mapped (NOT IMPLEMENTED: the PMM cannot know if a frame it frees is still mapped).
Reclaiming bootloader-reclaimable memory would be a new function that changes region types and clears bitmap bits — NOT IMPLEMENTED.

## Security properties (see `docs/VALIDATION_REPORT_1.0.0.md` for the review)

Checked arithmetic on every address computation; every free validated before mutation; never allocates kernel, framebuffer,
firmware, ACPI, bad, bootloader-reclaimable, below-1 MiB or bitmap frames; failure to initialize is fatal and safe.
KNOWN LIMITATIONS: pages are **not zeroed** (a future user-mode allocator must zero); no locking; no per-owner accounting,
so any kernel code can free any allocated frame.

## Deviations from the approved plan (reported to the owner)

1. **Invalid free rejects and counts instead of panicking** (plan decision D4). Rationale: the directive requires
   "double-free and invalid-free *detection*" and tests that exercise it, which a panic would make untestable in the same boot.
   The status code makes the caller responsible. A future debug build may turn rejection into a panic.
2. Status-code API plus instance API (not in the plan's minimal interface) so synthetic maps can be tested deterministically.
3. The full normalized map is kept inside `struct pmm` (3 KiB) to validate frees by region type.
4. Headers live in `kernel/memory/` per the directive; `-Ikernel` was added to the compiler flags.
