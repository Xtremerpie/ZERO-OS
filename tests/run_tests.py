#!/usr/bin/env python3
"""ZERO OS test harness (version strings are read from kernel/include/zero/version.h).

Builds a bootable image per scenario, boots it headless in QEMU/OVMF, captures the
serial log and checks the content plus QEMU's exit status. Nothing is mocked: every
check runs against the real kernel booting in a real (emulated) UEFI machine.

Usage: python3 tests/run_tests.py [--only NAME ...] [--repeat N] [--keep-logs]
Exit status 0 only if every selected test passed.
"""
import argparse
import os
import re
import socket
import struct
import subprocess
import sys
import time
import zlib
import hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# ZERO_BUILD_DIR lets the same suite test an alternative build (e.g. `make ubsan` -> build-ubsan).
BUILD = os.path.join(ROOT, os.environ.get("ZERO_BUILD_DIR", "build"))
TESTDIR = os.path.join(BUILD, "test")
KERNEL = os.path.join(BUILD, "zero-kernel.elf")

def _version():
    h = open(os.path.join(ROOT, "kernel", "include", "zero", "version.h")).read()
    return (re.search(r'ZERO_VERSION_STRING "([^"]+)"', h).group(1),
            re.search(r'ZERO_CODENAME "([^"]+)"', h).group(1))


VERSION, CODENAME = _version()
BOOT_COMPLETE = f"ZERO-OS {VERSION} boot complete"

EXIT_PASS = 33   # isa-debug-exit value 0x10 -> (0x10 << 1) | 1
EXIT_PANIC = 35  # isa-debug-exit value 0x11
ANSI = re.compile(r"\x1b\[[0-9;?=]*[A-Za-z]")

BOOT_REQUIRED = [
    f'ZERO OS {VERSION} "{CODENAME}"',
    "From Nothing. Built for Control.",
    "kernel entry reached in 64-bit long mode",
    "bootloader: Limine",
    "long mode active",
    "gdt: loaded",
    "idt: loaded",
    "memory map:",
    "memory map validated:",
    "pmm: initialized:",
    "[PASS] pmm: allocation test",
    "[PASS] vmm: kernel address space and CR3 initialized",
    "[PASS] heap: kernel dynamic memory allocator initialized",
    "[PASS] apic: Local APIC initialized and legacy PIC masked",
    "[PASS] timer: APIC periodic timer initialized",
    "[PASS] acpi: ACPI table checksumming, MADT parsing, and discovery verified",
    "[PASS] pci: PCI config space, bus enumeration, and BAR probing verified",
    "[PASS] input: event queue, scancode translation, modifiers, and mouse packets verified",
    "[PASS] block: sector read/write, multi-block I/O, bounds, and read-only checks verified",
    "[PASS] usb: standard descriptor packing, setup packets, and controller models verified",
    "[PASS] proc: process lifecycle, address space isolation, and threads verified",
    "[PASS] sched: preemptive round-robin scheduler and sleep timer queues verified",
    "[PASS] vfs: vnodes, file handles, path resolution, and RAMFS verified",
    "[PASS] ipc: ring-buffered inter-process message mailboxes verified",
    "[PASS] elf: ELF64 executable validation, segment mapping, and user stack verified",
    "[PASS] storage: storage architecture, partition tables, and cache verified",
    "[PASS] zerofs: all native ZEROFS filesystem operations verified",
    "[PASS] net: network architecture, drivers, protocols, and socket interfaces verified",
    "[PASS] desktop: color palette packing and RGB component extraction verified",
    "[PASS] desktop: framebuffer double buffering, clipping, and damage tracking verified",
    "[PASS] desktop: 2D drawing primitives (lines, rects, circles, alpha blend) verified",
    "[PASS] desktop: font typography, metrics, integer scaling, and UI glyphs verified",
    "[PASS] desktop: mouse cursor delta tracking, clamping, and button states verified",
    "[PASS] desktop: window manager creation, z-ordering, hit-testing, and states verified",
    "[PASS] desktop: native application framework and registry verified",
    "[PASS] desktop: login authentication state machine and dev mode verified",
    "[PASS] ecosystem: ZERO Package Manager (zpm) initialized",
    "[PASS] ecosystem: application configuration subsystem initialized",
    "[PASS] ecosystem: ZAPP 128-byte header validation verified",
    "[PASS] ecosystem: path traversal security defenses verified (7 attack vectors blocked)",
    "[PASS] ecosystem: CRC32 checksum integrity calculation verified",
    "[PASS] ecosystem: ZPM package repository and CLI dispatcher verified",
    "[PASS] ecosystem: persistent application configuration (/data/config) verified",
    "[PASS] ecosystem: desktop notification toast lifecycle verified",
    "[PASS] ecosystem: application registry verified",
    "[PASS] security: Central Security Architecture initialized",
    "[PASS] security: cryptographic engine & FIPS SHA-256 KATs validated",
    "[PASS] security: capability tokens creation & fine-grained authorization verified",
    "[PASS] security: capability attenuation and immediate revocation verified",
    "[PASS] security: process sandboxing, jail root isolation & syscall gating verified",
    "[PASS] security: filesystem POSIX DAC permissions & mode bits verified",
    "[PASS] security: stateful firewall rule matching and policy enforcement verified",
    "[PASS] security: structured audit event ring buffering & persistence verified",
    "[PASS] intelligence: ZERO Intelligence Subsystems initialized",
    "[PASS] intelligence: AI service coordinator & daemon lifecycle validated",
    "[PASS] intelligence: AI provider abstraction & local CPU micro-engine validated",
    "[PASS] intelligence: model manager & single-active-model policy validated",
    "[PASS] intelligence: permission-gated automation broker & approval gates validated",
    "[PASS] intelligence: assistant request engine & intent classification validated",
    "[PASS] intelligence: persistent memory & user preferences validated",
    "[PASS] intelligence: AI security audit integration & telemetry validated",
    "[PASS] gpu: graphics acceleration device abstraction active",
    "[PASS] audio: audio device subsystem initialized",
    "[PASS] gaming: ZERO Gaming Mode & low-latency scheduler active",
    "[PASS] ultimate: system performance profiles (NORMAL, GAMING, POWER_SAVER) validated",
    "[PASS] ultimate: dynamic game registration & automatic profile restoration validated",
    "[PASS] ultimate: low-latency scheduler policies & telemetry validated",
    "[PASS] ultimate: graphics device abstraction, capabilities & 2D blitter validated",
    "[PASS] ultimate: audio device abstraction, volume controls & PCM buffer validated",
    "[PASS] ultimate: AI resource coordination & background inference throttling validated",
    "[PASS] ultimate: gaming telemetry, 60 FPS tracking & config persistence validated",
    BOOT_COMPLETE,
]

# name, kernel cmdline, required substrings, forbidden substrings, expected QEMU exit status
CASES = [
    ("boot", "zero.test_exit=1", BOOT_REQUIRED, ["PANIC"], EXIT_PASS),
    ("exception_bp", "zero.test_exit=1 zero.selftest=bp",
     BOOT_REQUIRED + ["#BP breakpoint at rip=", "(handled, resuming)", "bp: PASS"], ["PANIC"], EXIT_PASS),
    ("exception_div0", "zero.test_exit=1 zero.selftest=div0",
     ["KERNEL PANIC", "vector 0x00 #DE Divide Error", "System halted."], ["boot complete"], EXIT_PANIC),
    ("exception_ud", "zero.test_exit=1 zero.selftest=ud",
     ["KERNEL PANIC", "vector 0x06 #UD Invalid Opcode"], ["boot complete"], EXIT_PANIC),
    ("exception_gp", "zero.test_exit=1 zero.selftest=gp",
     ["KERNEL PANIC", "vector 0x0d #GP General Protection"], ["boot complete"], EXIT_PANIC),
    ("exception_pf", "zero.test_exit=1 zero.selftest=pf",
     ["KERNEL PANIC", "vector 0x0e #PF Page Fault", "CR2 (fault address): 0x00000000dead0000"],
     ["boot complete"], EXIT_PANIC),
    ("exception_df", "zero.test_exit=1 zero.selftest=df",
     ["KERNEL PANIC", "vector 0x08 #DF Double Fault"], ["boot complete"], EXIT_PANIC),
    ("exception_nested", "zero.test_exit=1 zero.selftest=nested",
     ["KERNEL PANIC", "selftest nested: message follows:", "NESTED PANIC: fault while panicking"],
     ["boot complete", "System halted."], EXIT_PANIC),
    ("panic_handler", "zero.test_exit=1 zero.selftest=panic",
     ["KERNEL PANIC", "selftest panic: deliberate kernel panic", "System halted."],
     ["boot complete"], EXIT_PANIC),
    ("unknown_selftest", "zero.test_exit=1 zero.selftest=nonsense",
     ["unknown selftest 'nonsense' (ignored)", BOOT_COMPLETE], ["PANIC"], EXIT_PASS),
    # Fail-safe: a corrupt (overlapping) memory map must be rejected and the boot must END, never continue.
    ("memmap_overlap_failsafe", "zero.test_exit=1 zero.selftest=memmap_overlap",
     ["injecting an overlapping memory-map entry", "KERNEL PANIC", "memory map rejected: overlapping entries",
      "System halted."], ["pmm: initialized", "boot complete", "[PASS] pmm: allocation test"], EXIT_PANIC),
    # 1.0.0 Core Foundation Subsystem Tests
    ("vmm_tests", "zero.test_exit=1 zero.selftest=vmm",
     BOOT_REQUIRED + ["[PASS] vmm: canonical address validation", "[PASS] vmm: 4 KiB map", "[PASS] vmm: W^X permission enforcement"],
     ["PANIC"], EXIT_PASS),
    ("heap_tests", "zero.test_exit=1 zero.selftest=heap",
     BOOT_REQUIRED + ["[PASS] heap: 16-byte alignment", "[PASS] heap: 8 segregated size classes", "[PASS] heap: large multi-page allocation"],
     ["PANIC"], EXIT_PASS),
    ("lock_tests", "zero.test_exit=1 zero.selftest=lock",
     BOOT_REQUIRED + ["[PASS] lock: atomic acquire", "[PASS] lock: IRQ-safe critical section"],
     ["PANIC"], EXIT_PASS),
    ("timer_tests", "zero.test_exit=1 zero.selftest=timer",
     BOOT_REQUIRED + ["[PASS] apic: Local APIC discovery", "[PASS] timer: APIC periodic timer", "[PASS] timer: hardware timer interrupt delivery"],
     ["PANIC"], EXIT_PASS),
    ("core_foundation", "zero.test_exit=1 zero.selftest=core",
     BOOT_REQUIRED + ["ALL CORE FOUNDATION SELF-TESTS PASSED!"],
     ["PANIC"], EXIT_PASS),
]

PMM_TESTS = [
    "init", "memmap_validation", "bitmap_init", "single_alloc", "multi_alloc", "contiguous_alloc",
    "alignment", "exhaustion", "free", "double_free", "invalid_free", "reserved_protection",
    "kernel_region_protection", "bitmap_boundary", "memmap_overflow", "alloc_free_cycles",
    "stats_consistency", "repeated_init", "randomized_sequences", "exhaustion_recovery",
]
DEFAULT_MATRIX = "128M,256M,512M,1G"


def host_available_mb():
    try:
        for line in open("/proc/meminfo"):
            if line.startswith("MemAvailable:"):
                return int(line.split()[1]) // 1024
    except OSError:
        pass
    return 0


def size_mb(size):
    return int(size[:-1]) * (1024 if size[-1] in "Gg" else 1)


def df_stack_check(text):
    """The #DF handler must be running INSIDE the dedicated IST1 stack, not on the corrupted stack."""
    top = re.search(r"IST1 at 0x([0-9a-f]+)", text)
    sp = re.search(r"handler stack: 0x([0-9a-f]+)", text)
    if not top or not sp:
        return ["cannot find IST1 address / handler stack in output"]
    top, sp = int(top.group(1), 16), int(sp.group(1), 16)
    if not (top - 16384 <= sp < top):
        return [f"#DF handler stack 0x{sp:x} is outside IST1 [0x{top - 16384:x}, 0x{top:x})"]
    return []


EXTRA_CHECKS = {"exception_df": df_stack_check}

results = []  # (name, ok, detail)
skipped = []  # (name, reason): NOT RUN, never counted as passed
image_changes = []  # boots whose disk image differed afterwards (must stay empty)
boots_hashed = [0]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def record(name, ok, detail=""):
    results.append((name, ok, detail))
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f" - {detail}" if detail else ""), flush=True)


def clean(text):
    return ANSI.sub("", text).replace("\r", "")


def make_image(name, cmdline, kernel=KERNEL):
    os.makedirs(TESTDIR, exist_ok=True)
    img = os.path.join(TESTDIR, f"{name}.img")
    r = subprocess.run([os.path.join(ROOT, "scripts", "mkimage.sh"), img, cmdline, kernel],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"mkimage failed: {r.stderr.strip()}")
    return img


def boot(img, timeout, mem=None):
    mem = mem or os.environ.get("ZERO_TEST_MEM", "512M")  # e.g. ZERO_TEST_MEM=256M on small machines
    """Boot headless; returns (cleaned serial output, exit status or None on timeout, seconds)."""
    cmd = [os.path.join(ROOT, "scripts", "run-qemu.sh"), img, "--headless", "--mem", mem]
    before = sha256_file(img)
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    try:
        out, _ = p.communicate(timeout=timeout)
        status = p.returncode
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
        status = None
    boots_hashed[0] += 1
    if sha256_file(img) != before:
        image_changes.append(os.path.basename(img))
    return clean(out.decode("utf-8", "replace")), status, time.time() - t0


def check(name, text, status, secs, required, forbidden, want_status, keep):
    problems = []
    if name in EXTRA_CHECKS:
        problems += EXTRA_CHECKS[name](text)
    for s in required:
        if s not in text:
            problems.append(f"missing {s!r}")
    for s in forbidden:
        if s in text:
            problems.append(f"unexpected {s!r}")
    if status != want_status:
        problems.append(f"QEMU exit status {status} (wanted {want_status})")
    save_log(name, text, keep or bool(problems))
    record(name, not problems, f"{secs:.1f}s" if not problems else "; ".join(problems))
    if problems:
        print("    ---- serial output (last 25 lines) ----")
        for line in text.strip().splitlines()[-25:]:
            print("    | " + line)
    return not problems


def save_log(name, text, keep):
    if keep:
        os.makedirs(TESTDIR, exist_ok=True)
        with open(os.path.join(TESTDIR, f"{name}.log"), "w") as f:
            f.write(text)


def kernel_lines(text):
    """The kernel's own output, from the serial banner onward (firmware chatter removed)."""
    lines = text.splitlines()
    for i, l in enumerate(lines):
        if l.startswith("====="):
            return lines[i:]
    return []


def run_cases(selected, keep):
    for name, cmdline, req, forb, want in CASES:
        if selected and name not in selected:
            continue
        img = make_image(name, cmdline)
        text, status, secs = boot(img, timeout=90)
        check(name, text, status, secs, req, forb, want, keep)


def run_pmm_matrix(sizes, keep):
    """Boot the 20-test PMM suite in each guest size; every test must report [PASS] exactly once."""
    for size in sizes:
        name = f"pmm_selftests_{size}"
        need = size_mb(size) + 1536  # guest RAM + QEMU/OS overhead headroom
        if host_available_mb() < need:
            skipped.append((name, f"host has {host_available_mb()} MB available, needs ~{need} MB"))
            continue
        img = make_image(f"pmm_{size}", "zero.test_exit=1 zero.selftest=pmm")
        text, status, secs = boot(img, timeout=600, mem=size)
        passed = re.findall(r"^\[PASS\] pmm: (\w+)$", text, re.M)
        failed = re.findall(r"^\[FAIL\] pmm: (\w+)$", text, re.M)
        problems = []
        got = [t for t in passed if t != "allocation"]
        if got != PMM_TESTS:
            problems.append(f"tests passed {len(got)}/{len(PMM_TESTS)}; missing {sorted(set(PMM_TESTS) - set(got))}")
        if failed:
            problems.append(f"FAILED: {failed}")
        if "pmm: selftest: 20/20 passed" not in text:
            problems.append("no '20/20 passed' summary")
        if BOOT_COMPLETE not in text or "PANIC" in text:
            problems.append("boot did not complete cleanly")
        if status != EXIT_PASS:
            problems.append(f"QEMU exit status {status}")
        save_log(name, text, keep or bool(problems))
        m = re.search(r"pmm: initialized: (\d+) frames tracked, bitmap (\d+) bytes", text)
        record(name, not problems,
               (f"{len(got)}/20 PMM tests passed in {secs:.1f}s; bitmap {m.group(2)} B for {m.group(1)} frames"
                if m and not problems else "; ".join(problems)))
        if problems:
            for line in text.strip().splitlines()[-12:]:
                print("    | " + line)


def run_pmm_bench(sizes, keep):
    """Collect [BENCH] lines per guest size into build/test/bench_<size>.txt (nothing invented).
    Also guards against a regression found during 1.0.0 development: single-page allocation cost
    must not grow with guest memory size (it did when the run-length check was unbounded)."""
    per_op = {}
    for size in sizes:
        name = f"pmm_bench_{size}"
        need = size_mb(size) + 1536
        if host_available_mb() < need:
            skipped.append((name, f"host has {host_available_mb()} MB available, needs ~{need} MB"))
            continue
        img = make_image(f"bench_{size}", "zero.test_exit=1 zero.selftest=pmm_bench")
        text, status, secs = boot(img, timeout=300, mem=size)
        lines = [l for l in text.splitlines() if l.startswith("[BENCH]")]
        ok = status == EXIT_PASS and any("alloc 1 page" in l for l in lines) and not any("FAILED" in l for l in lines)
        os.makedirs(TESTDIR, exist_ok=True)
        with open(os.path.join(TESTDIR, f"bench_{size}.txt"), "w") as f:
            f.write("\n".join(lines) + "\n")
        record(name, ok, f"{len(lines)} measurements saved to {os.path.relpath(TESTDIR, ROOT)}/bench_{size}.txt" if ok
               else f"status {status}, {len(lines)} bench lines")
        m = re.search(r"alloc 1 page\s+\d+ cycles total,\s+(\d+) cycles/op", "\n".join(lines))
        if m:
            per_op[size] = int(m.group(1))
    if len(per_op) >= 2:
        lo, hi = min(per_op.values()), max(per_op.values())
        record("pmm_alloc_cost_independent_of_memory_size", hi <= 8 * max(lo, 1),
               "alloc-1-page cycles/op by guest size: " + ", ".join(f"{k}={v}" for k, v in per_op.items()))


def run_image_unmodified():
    ok = not image_changes
    record("image_unmodified", ok,
           f"disk image hash identical before/after all {boots_hashed[0]} QEMU boots (snapshot=on)" if ok
           else f"image changed during boot: {sorted(set(image_changes))}")


def run_page_permissions():
    """Boot, then read the LIVE page tables from QEMU (`info tlb`): the kernel must be W^X,
    supervisor-only, with at least one executable read-only page (text) and writable pages NX."""
    img = make_image("pgperm", "")
    sock_path = os.path.join(TESTDIR, "pg.sock")
    if os.path.exists(sock_path):
        os.remove(sock_path)
    cmd = [os.path.join(ROOT, "scripts", "run-qemu.sh"), img, "--headless", "--mem", os.environ.get("ZERO_TEST_MEM", "512M"), "--monitor", sock_path]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    os.set_blocking(p.stdout.fileno(), False)
    buf, deadline = b"", time.time() + 90
    while time.time() < deadline and b"boot complete" not in buf:
        try:
            chunk = p.stdout.read(4096)
        except BlockingIOError:
            chunk = None
        if chunk:
            buf += chunk
        else:
            time.sleep(0.1)
    tlb = ""
    if b"boot complete" in buf:
        time.sleep(1.0)
        try:
            s = socket.socket(socket.AF_UNIX)
            s.settimeout(5)
            s.connect(sock_path)
            s.recv(4096)
            s.sendall(b"info tlb\n")
            time.sleep(2.0)
            s.settimeout(1)
            try:
                while True:
                    d = s.recv(65536)
                    if not d:
                        break
                    tlb += d.decode(errors="replace")
            except OSError:
                pass
            s.sendall(b"quit\n")
            s.close()
        except OSError:
            pass
    p.kill()
    p.wait()

    # Line format: "ffffffff80003000: 000000001fe11000 X---A----"; flags: X=no-exec, U=user, W=writable.
    pages = []
    for line in tlb.splitlines():
        m = re.match(r"(ffffffff8[0-9a-f]{7}): [0-9a-f]+ (\S{9})", line.strip())
        if m:
            pages.append((int(m.group(1), 16), m.group(2)))
    problems = []
    if len(pages) < 4:
        problems.append(f"only {len(pages)} kernel pages found in `info tlb`")
    for addr, fl in pages:
        nx, user, writable = fl[0] == "X", fl[7] == "U", fl[8] == "W"
        if writable and not nx:
            problems.append(f"page 0x{addr:x} is writable AND executable ({fl})")
        if user:
            problems.append(f"page 0x{addr:x} is user-accessible ({fl})")
    if pages and not any(fl[0] != "X" and fl[8] != "W" for _, fl in pages):
        problems.append("no executable read-only (text) page found")
    n_text = sum(1 for _, fl in pages if fl[0] != "X" and fl[8] != "W")
    n_ro = sum(1 for _, fl in pages if fl[0] == "X" and fl[8] != "W")
    n_rw = sum(1 for _, fl in pages if fl[0] == "X" and fl[8] == "W")
    record("kernel_wx_mapping", not problems,
           f"{len(pages)} kernel pages: {n_text} R-X, {n_ro} R--, {n_rw} RW-NX, none W+X, none user" if not problems
           else "; ".join(problems[:3]))


def run_test_exit_strict(keep):
    """`zero.test_exit=10` must NOT enable the QEMU exit device (only exactly "1" does)."""
    img = make_image("test_exit_strict", "zero.test_exit=10")
    text, status, secs = boot(img, timeout=25)
    save_log("test_exit_strict", text, keep)
    ok = BOOT_COMPLETE in text and status is None  # still running = no exit signal
    record("test_exit_strict_parsing", ok,
           "'zero.test_exit=10' did not trigger the exit device" if ok
           else f"unexpected: status={status}, boot complete={'boot complete' in text}")


def run_repeatability(n, keep):
    """Boot the identical image n times: kernel output must be byte-identical."""
    img = make_image("repeat", "zero.test_exit=1")
    outputs, times = [], []
    for _ in range(n):
        text, status, secs = boot(img, timeout=90)
        if status != EXIT_PASS:
            save_log("repeat_fail", text, True)
            record("repeatable_startup", False, f"a boot ended with status {status}")
            return
        outputs.append("\n".join(kernel_lines(text)))
        times.append(secs)
    save_log("repeat", outputs[0], keep)

    # Known, measured environment variance (docs/TESTING.md): Limine/OVMF lay out their own
    # allocations differently between boots. Measured over 12 boots, exactly these kernel output
    # lines vary: CR3 value, bootloader entry count (30/32), the usable vs bootloader-reclaimable
    # split (52 KiB), and the PMM numbers derived from them. The usable+reclaimable SUM must stay
    # constant, and every other kernel line must be byte-identical.
    split_re = re.compile(r"^\[INFO \] (mem|memory):   (usable|bootloader-reclaimable)\s+(\d+) KiB$")
    pmm_total_re = re.compile(r"(total \d+ KiB), usable \d+ KiB, reserved \d+ KiB")
    # The bitmap covers 0..highest usable end, which moves with the same firmware layout variance.
    pmm_init_re = re.compile(r"initialized: \d+ frames tracked, bitmap \d+ bytes")
    # Also measured to vary (12/12 boots examined): the page-table root Limine allocates, and the number
    # of bootloader map entries (30 or 32) as it splits its own allocations differently.
    cr3_re = re.compile(r"CR3=0x[0-9a-f]+")
    entries_re = re.compile(r"(memory map: |memory map validated: )\d+ (entries from bootloader|bootloader entries -> )\d*(?: regions)?")
    pmm_alloc_re = re.compile(r"allocatable \d+ KiB \(\d+ frames\), free \d+ frames")
    # APIC timer delivery is real-time: interrupt counts naturally differ by a few
    # ticks between otherwise identical boots, so compare the invariant message.
    timer_ticks_re = re.compile(r"(timer_test: ticks after spin: )\d+( \(delta: )\d+(\))")
    timer_pass_re = re.compile(r"(\[PASS\] timer: hardware timer interrupt delivery \(ticks: )\d+(\))")
    # Uptime is measured from the current boot and naturally varies between starts.
    uptime_re = re.compile(r"(timer_test: current system uptime: )\d+( ms)")
    uptime_pass_re = re.compile(r"(\[PASS\] timer: uptime calculation \()\d+( ms\))")
    # Gaming test lifecycle duration is timer-driven and can differ by one tick.
    gaming_runtime_re = re.compile(r"(unregistered game 'TestGame88' \(PID: 88, runtime: )\d+( ms\))")
    # Firmware allocation timing can shift the bootloader free-frame count.
    usable_frames_re = re.compile(r"(Usable RAM:\s+\d+ MiB \()\d+( frames free\))")

    def normalise(output):
        kept, total = [], 0
        for line in output.splitlines():
            # QEMU writes host-side diagnostics to the same pipe as serial output;
            # their arrival order relative to guest UART lines is nondeterministic.
            if line.startswith("qemu-system-x86_64:"):
                continue
            m = split_re.match(line)
            if m:
                total += int(m.group(3)) if m.group(1) == "mem" else 0  # sum checked on raw bootloader lines
                kept.append(f"[INFO ] {m.group(1)}:   {m.group(2)} <varies>")
            else:
                line = pmm_total_re.sub(r"\1, usable <varies>, reserved <varies>", line)
                line = cr3_re.sub("CR3=<varies>", line)
                line = entries_re.sub(lambda m: m.group(1) + "<varies> " + m.group(2).rstrip(" ->") + (" <varies> regions" if "validated" in m.group(1) else ""), line)
                line = pmm_init_re.sub("initialized: <varies> frames tracked, bitmap <varies> bytes", line)
                line = pmm_alloc_re.sub("allocatable <varies>, free <varies>", line)
                line = timer_ticks_re.sub(r"\1<varies>\2<varies>\3", line)
                line = timer_pass_re.sub(r"\1<varies>\2", line)
                line = uptime_re.sub(r"\1<varies>\2", line)
                line = uptime_pass_re.sub(r"\1<varies>\2", line)
                line = gaming_runtime_re.sub(r"\1<varies>\2", line)
                line = usable_frames_re.sub(r"\1<varies>\2", line)
                kept.append(line)
        return "\n".join(kept), total

    norm = [normalise(o) for o in outputs]
    texts = {t for t, _ in norm}
    sums = {s for _, s in norm}
    raw_variants = len(set(outputs))
    if len(texts) == 1 and len(sums) == 1 and 0 not in sums:
        record("repeatable_startup", True,
               f"{n} boots; all kernel output identical except the usable/reclaimable split "
               f"({raw_variants} raw variant(s), sum constant at {sums.pop()} KiB); "
               f"wall time {min(times):.1f}-{max(times):.1f}s")
    else:
        a, b = [o.splitlines() for o in list(texts)[:2]] if len(texts) > 1 else ([], [])
        diff = [f"{x!r} != {y!r}" for x, y in zip(a, b) if x != y]
        record("repeatable_startup", False,
               f"{len(texts)} distinct normalised outputs, sums={sorted(sums)}; first diff: {diff[:2]}")


def run_failure_diagnostics(keep):
    """A kernel that cannot boot must be reported as a failure, never as a pass."""
    junk = os.path.join(TESTDIR, "not-a-kernel.elf")
    os.makedirs(TESTDIR, exist_ok=True)
    with open(junk, "wb") as f:
        f.write(b"this is not an ELF file\n" * 64)
    img = make_image("bad_kernel", "zero.test_exit=1", kernel=junk)
    text, status, secs = boot(img, timeout=45)
    booted = f"ZERO OS {VERSION}" in text
    save_log("bad_kernel", text, True)
    if not booted and status != EXIT_PASS:
        how = "timeout (no kernel output)" if status is None else f"QEMU exit {status}"
        record("failure_diagnostics", True,
               f"corrupt kernel detected as boot failure via {how}; log kept in build/test/bad_kernel.log")
    else:
        record("failure_diagnostics", False, "a corrupt kernel was NOT detected as a failure")


def png_write(path, w, h, rows):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + r for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def run_framebuffer():
    """Boot, wait for 'boot complete', take a real QEMU screendump, verify the banner pixels."""
    img = make_image("framebuffer", "")
    sock_path = os.path.join(TESTDIR, "mon.sock")
    ppm = os.path.join(TESTDIR, "screen.ppm")
    for p in (sock_path, ppm):
        if os.path.exists(p):
            os.remove(p)
    cmd = [os.path.join(ROOT, "scripts", "run-qemu.sh"), img, "--headless", "--mem", os.environ.get("ZERO_TEST_MEM", "512M"), "--monitor", sock_path]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    os.set_blocking(p.stdout.fileno(), False)
    buf, deadline = b"", time.time() + 90
    while time.time() < deadline and b"boot complete" not in buf:
        try:
            chunk = p.stdout.read(4096)
        except BlockingIOError:
            chunk = None
        if chunk:
            buf += chunk
        else:
            time.sleep(0.1)
    ok_boot = b"boot complete" in buf
    shot = False
    if ok_boot:
        time.sleep(1.0)
        try:
            s = socket.socket(socket.AF_UNIX)
            s.settimeout(5)
            s.connect(sock_path)
            s.recv(4096)
            s.sendall(f"screendump {ppm}\n".encode())
            time.sleep(1.5)
            s.sendall(b"quit\n")
            s.close()
            shot = os.path.exists(ppm)
        except OSError:
            shot = False
    p.kill()
    p.wait()
    if not (ok_boot and shot):
        record("framebuffer_banner", False, "kernel did not complete boot or screendump failed")
        return
    data = open(ppm, "rb").read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    w, h = int(m.group(1)), int(m.group(2))
    px = data[m.end():]
    rows = [px[y * w * 3:(y + 1) * w * 3] for y in range(h)]
    png_write(os.path.join(TESTDIR, "framebuffer.png"), w, h, rows)

    def count(rgb):
        needle = bytes(rgb)
        return sum(r.count(needle) for r in rows)  # approximate (unaligned matches are negligible)

    gold, crimson, white = count((0xC9, 0xA2, 0x27)), count((0xB3, 0x20, 0x2B)), count((0xE8, 0xE8, 0xE8))
    ok = (w, h) == (1280, 800) and gold > 1000 and crimson > 500 and white > 1000
    record("framebuffer_banner", ok,
           f"{w}x{h}; banner pixels gold={gold} crimson={crimson} white={white}; "
           f"screenshot: build/test/framebuffer.png")


def run_version_manifest():
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tests", "check_versions.py")],
                       capture_output=True, text=True)
    record("version_manifest", r.returncode == 0, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", nargs="*", help="run only these case names (or: version, pmm, bench, wx, strict, repeat, failure, framebuffer, image)")
    ap.add_argument("--repeat", type=int, default=5)
    ap.add_argument("--matrix", default=os.environ.get("ZERO_PMM_MATRIX", DEFAULT_MATRIX),
                    help="comma-separated guest sizes for the PMM suite (default %(default)s). "
                         "4G only runs when listed explicitly AND the host has the memory.")
    ap.add_argument("--keep-logs", action="store_true")
    a = ap.parse_args()

    if not os.path.exists(KERNEL):
        print(f"kernel not built: {KERNEL} (run 'make')", file=sys.stderr)
        return 2

    want = set(a.only or [])
    print(f"ZERO OS {VERSION} \"{CODENAME}\" test run")
    if not want or "version" in want:
        run_version_manifest()
    run_cases(want, a.keep_logs)
    sizes = [x.strip() for x in a.matrix.split(",") if x.strip()]
    if not want or "pmm" in want:
        run_pmm_matrix(sizes, a.keep_logs)
    if not want or "bench" in want:
        run_pmm_bench(sizes, a.keep_logs)
    if not want or "wx" in want:
        run_page_permissions()
    if not want or "strict" in want:
        run_test_exit_strict(a.keep_logs)
    if not want or "repeat" in want:
        run_repeatability(a.repeat, a.keep_logs)
    if not want or "failure" in want:
        run_failure_diagnostics(a.keep_logs)
    if not want or "framebuffer" in want:
        run_framebuffer()

    if not want or "image" in want:
        run_image_unmodified()

    for n, why in skipped:
        print(f"  [NOT RUN] {n} - {why}")
    failed = [n for n, ok, _ in results if not ok]
    print(f"\n{len(results) - len(failed)} passed, {len(failed)} failed, {len(skipped)} not run")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
