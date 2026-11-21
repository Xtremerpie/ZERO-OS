# Kernel Architecture

Monolithic-leaning kernel with strict internal interfaces, so that services can be moved out
of the kernel later where it pays off (ZERO CORE services are modular). Language: C11
freestanding + NASM. Target: x86_64 only.

## Build properties [implemented]

- Higher-half at `0xffffffff80000000`, `-mcmodel=kernel`, non-PIC, no red zone,
  `-mgeneral-regs-only` (no SSE/FPU use in the kernel), `-fno-builtin`, `-Werror`.
- Segments are page-aligned and separated: R-X text, R-- rodata, RW- data/bss. **Verified** on the live page
  tables in QEMU (`kernel_wx_mapping` test): no kernel page is writable and executable; none is user-accessible.
  These tables are Limine's; ZERO builds its own from 1.0.0.
- Size at 0.0.0: see `docs/PERFORMANCE.md` (text+rodata 15,364 B, data 336 B, bss 20,752 B; bss includes the 16 KiB #DF stack).
- Compile-time ABI guards: `_Static_assert`s pin the `interrupt_frame` layout to `isr.asm`, and the GDT/TSS/IDT-gate sizes.

## Module interfaces (public headers: `kernel/include/zero/`)

| Header | Module | State |
|--------|--------|-------|
| `serial.h` | COM1 UART | implemented |
| `fbcon.h` | framebuffer text/banner | implemented |
| `log.h` | `kprintf`, `klog` | implemented |
| `cpu.h` | port I/O, CRs, MSRs, CPUID | implemented |
| `gdt.h` / `idt.h` | descriptor tables, exception dispatch | implemented |
| `panic.h` | panic, exception panic | implemented |
| `selftest.h` | fault injection, QEMU exit device | implemented |
| `string.h` | mem*/str* | implemented |
| `version.h` | version identity | implemented |

## Planned core (1.0.0) [designed]

PMM → VMM → heap → LAPIC timer/interrupts → threads → scheduler → syscalls. Rules:
kernel code never trusts user pointers (validated copy-in/out), syscall table is versioned,
every allocation path has a failure path, locks have documented ordering.

## Exception model [implemented+tested]

Vectors 0–31 → `isr_stub_N` (asm) pushes a uniform `interrupt_frame` → `exception_dispatch()`.
`#BP` is the only recoverable exception (used as a test of the resume path). `#DF` runs on a
dedicated IST1 stack. Interrupts stay masked (`IF=0`); no IRQs exist yet.
A fault inside the panic path is reported on serial only and halts (`exception_nested` test).

## Known hardening gaps (deliberate, tracked for 1.0.0)

- NMI and #MC use the normal kernel stack (no IST); an NMI during early boot panics. Not claimed to be handled.
- The IST1 (#DF) stack has no guard page: it is 16 KiB of static bss, and a stack overflow past it would corrupt adjacent data. Guard pages need the 1.0.0 VMM.
- Segment loads zero FS/GS; per-CPU GS-base setup (1.0.0) must come *after* `gdt_init()`.
- `kvprintf` trusts `%s` pointers (the panic path survives a bad one only by the nested-panic guard).
- Single CPU only; panic/log paths are not SMP-safe.
