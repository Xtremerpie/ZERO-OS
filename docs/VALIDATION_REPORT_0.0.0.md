# 0.0.0 Validation and Handover Report

Scope: review of the delivered 0.0.0 "Genesis" repository before any Version 1.0.0 work. No PMM code exists.
The official version manifest is unchanged (`git diff 7b2cc80 -- configs/versions.json tests/check_versions.py` is empty; exactly ten entries).

## Checklist result

| # | Item | Result |
|---|------|--------|
| 1 | Review the complete repository again | Done: every kernel C/asm file, linker script, Makefile, scripts, harness and all docs re-read; 11 defects/gaps found and fixed (below), 6 limits documented |
| 2 | Accidental generated files / build artifacts in version control | None tracked. `git ls-files` = source, docs, configs, scripts, tests and the 3 pinned third-party files only. `build/`, `build-ubsan/`, `__pycache__/` are untracked |
| 3 | Verify `.gitignore` | Verified with `git check-ignore` (objects, `.elf`, `.img`, `.iso`, `.ppm`, logs, editor dirs ignored; `BOOTX64.EFI`, linker script, docs, manifest *not* ignored). Added `build-ubsan/`, `*.sock`, `*.fd`, `*.tar.gz`, `*.zip`. Added `.gitattributes` (LF endings) |
| 4 | Verify third-party licences and hashes | `sha256sum -c`: 3/3 OK; clone HEADs equal the pinned commits; font identical to upstream apart from the one documented edit; Limine licence is BSD 2-Clause (2 clauses present), protocol header 0BSD. Recorded in `third_party/THIRD_PARTY.md`. Corrected: "fresh checkout" wording, removed unused `dosfstools` |
| 5 | Memory-safety / undefined-behaviour review | See "Kernel findings": clang static analyzer (0 findings on 10 files; proven able to detect seeded bugs), UBSan trap build (42 check sites, none fired in 16 tests), manual review |
| 6 | Exception and double-fault review | See "Exception review" |
| 7 | Documentation accuracy | Every factual claim re-checked against fresh evidence; 6 corrections (below) |
| 8 | Official manifest unchanged | Confirmed (see above) |
| 9 | No extra official versions | Confirmed; the new work is labelled "hardening review (still 0.0.0)" |
| 10 | PMM not started | Confirmed: no `pmm` file or symbol exists (`git grep -i pmm -- kernel` is empty); plan only in `docs/PMM_PLAN_1.0.0.md` |

## Kernel findings and fixes

| ID | Severity | Where | Problem | Fix | Verification |
|----|----------|-------|---------|-----|--------------|
| F1 | Medium | `cpu.c` | `char` arrays accessed as `uint32_t*` for CPUID strings: strict-aliasing and alignment UB | build strings from typed words + `memcpy` | analyzer + UBSan + review (UBSan does not detect aliasing; this one rests on review) |
| F2 | Low | `cpu.c` | extended-model used for base family ≥ 6 instead of exactly 6 or 0xF | rule corrected per the CPUID definition | boot log unchanged on QEMU (15/107/1); logic review |
| F3 | Low-Med | `gdt.c` | GDT/TSS are `packed` structs ⇒ alignment 1 | `aligned(16)` + `_Static_assert` on sizes | build + all tests |
| F4 | Medium | Makefile | build id passed as a `-D` flag: not a dependency, binaries could carry a stale id | generated header, rewritten only on change; `-MMD` rebuilds exactly the 2 objects that include it | rebuilt after a commit without `clean`: only `panic.o`, `main.o` recompiled, new id present, old id absent |
| F5 | Low | `selftest.c` | `zero.test_exit=10` accepted as `1` | exact match | new test `test_exit_strict_parsing` |
| F6 | Medium | `panic.c` | a fault inside the panic path halted silently with no message | serial-only "NESTED PANIC" report, then halt/exit | new selftest/test `exception_nested` |
| F7 | Low | `main.c` | NULL `cmdline` pointer from the bootloader would be dereferenced | guard | review (not reachable on Limine 11.4.1) |
| F8 | Low | `fbcon.c` | clip arithmetic `x + w` could overflow; pixel writes assume 4-byte alignment | overflow-safe compare; refuse unaligned framebuffer (serial-only fallback) | review; not reachable from current callers |
| F9 | Low | `mkimage.sh` | accepted symlinks/char devices as output and newlines in the cmdline | refuses any non-regular-file output and multi-line cmdline | 4 refusals exercised, normal build still works |
| F10 | Medium (Windows) | repo | no `.gitattributes`: a Windows checkout with autocrlf would turn `*.sh` into CRLF and break them in WSL | `* text=auto eol=lf` + explicit rules | files verified CR-free; rule added |
| F11 | Low | `idt.c` | asm/C frame layout was an unchecked ABI | six `_Static_assert`s on offsets and size | build |

Accepted limits (documented in `docs/KERNEL_ARCHITECTURE.md` "Known hardening gaps"): NMI/#MC without IST; no guard page under the #DF stack;
`kvprintf` trusts `%s`; FS/GS zeroed at `gdt_init`; single-CPU only; Limine `quiet` hides boot-failure causes.

## Exception and double-fault review

- Vector table, error-code set (8, 10–14, 17, 21, 29, 30 push one; the rest get a pushed 0) and the uniform frame in `isr.asm` re-checked; offsets now compile-time asserted.
- Stack: 16-byte alignment before `call`; `rbp` saved/restored through the frame; `cld` on entry; `iretq` restores flags/segments. `#BP` returns and execution resumes (tested).
- `#DF`: gate uses IST1; TSS descriptor bit layout re-derived and `ltr` works. **Now proven, not assumed:** with RSP deliberately set to `0x1000`, the handler prints `handler stack: 0xffffffff80009f90`, inside the IST1 range ending `0xffffffff8000a070`; the test asserts that range.
- Panic: recursion guarded; nested fault reported (new). Mutation check: removing IST for `#DF` fails `exception_df` (CPU triple-faults, QEMU resets).
- Memory permissions: kernel mapping verified on the live page tables — 3 R-X, 2 R--, 7 RW-NX pages, none writable+executable, none user-accessible. Mutation: an RWX text segment fails `kernel_wx_mapping`.

## Documentation corrections

1. `W^X` was "not independently verified" → now verified and tested.
2. Performance wall times "2.5–2.7 s (8 boots)" → "2.2–3.9 s across all runs, ~2.8–3.1 typical".
3. Kernel size 71,096 → 72,264 bytes; section sizes updated (text+rodata 15,348 B at the final commit; data 336; bss 20,752).
4. THIRD_PARTY: "fresh checkout" and unused `dosfstools` wording fixed.
5. TESTING.md: new tests, quality gates and mutation checks; test count 13 → 16.
6. BUILDING.md: build-id behaviour, `make analyze`/`ubsan`, pointer to the Windows guide. Host/tool versions, HHDM base, base revision 6, 461/4045 MiB usable all re-confirmed against fresh runs.

## Final evidence (commit `677ce28` + documentation commits)

| Gate | Result |
|------|--------|
| `make clean && make` | 0 warnings under `-Wall -Wextra -Werror` |
| `make analyze` | 10 files, 0 findings |
| `make test`, 512 MB guest | 16 passed, 0 failed |
| `ZERO_TEST_MEM=256M make test` | 16 passed, 0 failed |
| `make ubsan` + suite (`ZERO_BUILD_DIR=build-ubsan`) | 16 passed, 0 failed |
| Guest RAM floor | 128 MB boots; 96 MB does not (firmware stage) |
| QEMU peak memory | ≈ 190 MB for 128–512 MB guests |

## Not verified (stated plainly)

Windows 11 / J3355: nothing was run there. The Windows guide's QEMU arguments are the Linux-verified ones; the PowerShell wrappers and the
Windows QEMU install paths/package id are untested. Real hardware: untested. Strict-aliasing correctness (F1) rests on review, not a tool.
