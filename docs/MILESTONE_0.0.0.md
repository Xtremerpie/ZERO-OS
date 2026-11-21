# Milestone 0.0.0 — Genesis

**Status: acceptance criteria met in QEMU on Ubuntu 24.04.** Two items remain on the user's side
(see "Open items"). Not run on real hardware or on Windows.

## Goals
Establish ZERO OS correctly: architecture documented, repository and toolchain in place, a real
kernel of our own that boots in QEMU, prints its identity, logs diagnostics, and handles CPU exceptions.

## Features (all verified by `make test`, 16/16 passing on a clean rebuild)
Boot via UEFI+Limine · long-mode entry · COM1 serial log · framebuffer banner + log console · CPUID/CR/EFER
report · own GDT/TSS · own IDT (vectors 0–31) with resumable `#BP` · panic handler with register dump ·
fault-injection selftests (incl. nested-panic) · memory-map reporting · QEMU exit device for tests.
A post-delivery validation review (`docs/VALIDATION_REPORT_0.0.0.md`) fixed 11 defects/gaps and added quality gates (static analysis, UBSan build, live page-table W^X test).

## Architecture changes
Initial architecture only — see `ARCHITECTURE.md`.

## Acceptance criteria and evidence

| # | Criterion | Evidence |
|---|-----------|----------|
| 1 | Documented technical architecture | `ARCHITECTURE.md`, `docs/*` (boot, kernel, memory, hardware plan, graphics, threat model, application, performance, design, conventions, `TESTING.md`) |
| 2 | Repository initialized | git repo, commit `7b2cc80`, structure per directive |
| 3 | Toolchain reproducible or fully documented | `BUILDING.md` lists exact versions; Limine/header/font pinned by commit + SHA-256 (`third_party/THIRD_PARTY.md`); `scripts/check-env.sh`. *Documented, not bit-for-bit reproducible* |
| 4 | Own kernel compiles | clean `make`: 0 warnings under `-Wall -Wextra -Werror`, 72,264-byte ELF; `make analyze` 0 findings |
| 5 | Boots in QEMU | `boot` test, plus a 4 GB-guest boot (4045 MiB usable), exit status 33 |
| 6 | Displays its own boot identity | serial banner (`ZERO OS 0.0.0 "Genesis"`) + `framebuffer_banner` test with real screendump |
| 7 | Produces diagnostic logs | serial + framebuffer log; `build/test/*.log` |
| 8 | Implemented exception handling tested | `#BP` (handled+resumed), `#DE`, `#UD`, `#GP`, `#PF` (CR2 checked), `#DF` (handler stack proven inside IST1), nested panic; panic handler. Mutation check: removing the `#DF` IST makes the test fail |
| 9 | Build process documented | `BUILDING.md`, `Makefile`, `scripts/` |
| 10 | No existing OS kernel used | kernel is original code (`kernel/`); only Limine (bootloader), its protocol header and a public-domain font are third-party |

## Test results (final clean rebuild)

`version_manifest, boot, exception_bp, exception_div0, exception_ud, exception_gp, exception_pf, exception_df, exception_nested,
panic_handler, unknown_selftest, kernel_wx_mapping, test_exit_strict_parsing, repeatable_startup (5 boots), failure_diagnostics, framebuffer_banner` — **16 passed, 0 failed**
(512 MB guest; also 16/16 with a 256 MB guest and 16/16 on the UBSan build).
Also checked manually: 4 GB-guest boot; GDB source-level debugging (`kmain`, `exception_dispatch`); mutation tests (no #DF IST; RWX text).

## What does not work / is incomplete

- No memory manager, interrupts/timer, scheduler, user mode, drivers (beyond serial and bootloader framebuffer), filesystem, etc. — later milestones.
- Never run on real hardware (including the Celeron J3355) or on Windows.
- Secure Boot unsupported (Limine unsigned).
- Framebuffer console is minimal: 8x8 font, clear-and-restart when full, no scrolling.

## Known limitations / bugs

1. Usable vs bootloader-reclaimable memory split varies by 52 KiB between boots (firmware/bootloader layout). Test asserts constant sum.
2. Bootloader-stage failure *causes* are not diagnosed (Limine runs `quiet`); harness only detects the failure.
3. Build ID embeds `git describe`, so binaries differ per commit; builds are not bit-for-bit reproducible (FAT timestamps).
4. `scripts/check-env.ps1` and the Windows instructions in `BUILDING.md` are untested.
5. Kernel W^X is verified only for Limine's page tables in QEMU; ZERO builds its own tables from 1.0.0 (guard pages for the #DF stack, NMI/#MC IST: see `docs/KERNEL_ARCHITECTURE.md`).
6. `LICENSE` is a placeholder pending the owner's choice.

## Security review
No security features exist. Review limited to: no secrets in repo; `mkimage.sh` refuses block devices and
only writes a named file; nothing touches physical disks; third-party binary hashed and pinned; tests run in an isolated VM.
See `docs/THREAT_MODEL.md`.

## Performance review
Kernel ELF 72,264 bytes (text+rodata 15,348 / data 336 / bss 20,752). Full QEMU run incl. emulated firmware ≈ 2.2–3.9 s wall (~2.8 typical). Smallest bootable guest: 128 MB; QEMU peak ≈ 190 MB.
No timer yet → no boot-time measurement; nothing measured on target hardware. See `docs/PERFORMANCE.md`.

## Release notes
ZERO OS 0.0.0 "Genesis": first bootable kernel. A developer milestone for QEMU, not usable as a desktop OS.

## Open items (user side)
1. **Choose a licence** and replace `LICENSE`.
2. **Validate on your Windows 11 / J3355 machine** with `docs/WINDOWS_VALIDATION.md` (Path A: prebuilt image in QEMU, no WSL, no hypervisor; Path B optional: full build via WSL2) and report the results. I could not test the Windows side.

## Next engineering task (first task of 1.0.0, after the open items above)

**Physical Memory Manager — now done; see `docs/MILESTONE_1.0.0.md`.** (Original note:) the exact implementation plan — design decisions, interface, algorithms, files, test matrix,
mutation checks, acceptance criteria, risks — is in `docs/PMM_PLAN_1.0.0.md`. *(Historical: written before approval.)*
