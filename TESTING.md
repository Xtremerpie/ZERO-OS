# Testing

`make test` (= `python3 tests/run_tests.py`) builds an image per scenario, boots it headless
in QEMU+OVMF, captures the serial log and checks (a) required/forbidden text and (b) QEMU's
exit status. The kernel signals pass/panic through QEMU's `isa-debug-exit` device, only when its
command line contains `zero.test_exit=1` — a normal boot never touches that port.

`python3 tests/test_hardware.py` executes the standalone hardware unit test suite verifying ACPI checksumming, MADT parsing, PCI address and BAR sizing math, PS/2 scancode decoding, mouse 3-byte packets, block boundary checks, and USB binary descriptor packing.

`python3 tests/test_storage_net.py` executes the standalone storage and networking test suite verifying MBR/GPT layouts, IEEE 802.3 CRC32, ZEROFS Superblock/Inode/Dirent layouts, Ethernet II, ARP, RFC 791 IPv4 checksumming, ICMP Echo, UDP, DHCP client state machine, DNS QNAME encoding, TCP RFC 793 state machine, and Socket API syscall bindings.

`python3 tests/test_desktop.py` executes the standalone ZERO Desktop test suite verifying color bit-packing, alpha blending, clipping containment, Bresenham line rasterization, font metrics and UI glyphs, window manager z-ordering and hit-testing, compositor 7-layer scene invariants, terminal command execution, file manager paths, and control center tabs.

`python3 tests/test_ecosystem.py` executes the standalone ZERO Ecosystem test suite verifying .zapp binary headers (128B), manifests (384B), file entries (144B), CRC32 and SHA256 cryptographic verification, path traversal attack defenses (7 attack vectors), ZPM package repository and search, application configuration parser, and ZERO SDK headers.

`python3 tests/test_security.py` executes the standalone ZERO Security test suite verifying capability tokens, attenuation and revocation, process sandboxing containment, user-pointer canonical validation, POSIX DAC permissions (0777 mode bits), FIPS SHA-256 Known Answer Tests, timing-safe memory comparisons, stateful firewall rule priority matching, and security audit event logging.

`python3 tests/test_intelligence.py` executes the standalone ZERO Intelligence test suite verifying AI assistant daemon states and ring buffer logging, pluggable provider abstraction, local model manager and single-active-model memory budgets, permission-gated automation broker with interactive User Approval Gates, assistant request engine context bounds, persistent memory, and security audit telemetry.

`python3 tests/test_ultimate.py` executes the standalone ZERO Ultimate test suite verifying ZERO Gaming Mode profiles, dynamic game process prioritization and auto-restoration, low-latency scheduler time-slice policies and starvation prevention, graphics device abstraction and capability discovery, audio device PCM buffering and volume limits, AI assistant resource throttling during games, and 60 FPS performance HUD telemetry.

`python3 tests/benchmark_suite.py` executes the repeatable micro-benchmark suite measuring memory allocation, cryptographic hashing, scheduler context-switching, 2D graphics blitting, and block cache lookups.

| Test | What it proves |
|------|----------------|
| `version_manifest` | exactly ten official versions, correct order, kernel version matches |
| `boot` | UEFI→Limine→kernel entry, banner, long mode active, GDT/IDT loaded, memory map reported, boot complete; exit 33 |
| `exception_bp` | `int32` is taken, handled, **resumed**; kernel continues to boot complete |
| `exception_div0` / `_ud` / `_gp` / `_pf` / `_df` | each CPU fault reaches the right vector, prints the register dump, halts; `#PF` shows the correct `CR2`; `#DF` is delivered on the separate IST1 stack |
| `exception_nested` | a fault *inside* the panic path gives a serial-only "NESTED PANIC" report and halts |
| `panic_handler` | `kpanic()` prints message, halts, exit 35 |
| `kernel_wx_mapping` | reads the **live page tables**: kernel pages are W^X and supervisor-only |
| `pmm_selftests_<size>` | the 20 PMM tests in guests of 128M, 256M, 512M, 1G |
| `vmm_tests` | 4-level paging, canonical address validation, 4 KiB map/translate/unmap, virtual write/physical read, $W \oplus X$ permission enforcement |
| `heap_tests` | kernel dynamic heap allocator: 16-byte alignment, 8 segregated size classes (16B - 2048B), large page allocation, canaries, zero-leak invariant |
| `lock_tests` | atomic test-and-set spinlocks, non-blocking try-acquire, and IRQ-safe state preservation |
| `timer_tests` | Local APIC discovery, periodic timer tick delivery on vector `0x20` via `sti`, and uptime tracking |
| `proc_tests` | Process lifecycle, private PML4 tables, kernel/user threads, cleanup, and count invariance |
| `sched_tests` | Round-robin preemptive scheduler, idle thread, sleep queues, and tick progression |
| `vfs_tests` | Vnodes, file handles, path resolution, RAMFS dynamic read/write/seek/unlink |
| `ipc_tests` | Ring-buffered inter-process message mailboxes, unicast delivery, and queue capacity limit |
| `elf_tests` | ELF64 executable header validation, segment mapping with W^X, and user stack allocation |
| `acpi_tests` | Modular 8-bit checksumming, MADT record parsing (CPUs, IO-APICs, ISOs), and HPET discovery |
| `pci_tests` | Type 1 configuration space access, bus 0..255 enumeration, and safe non-destructive BAR probing |
| `input_tests` | Unified input event queue, Scancode Set 1 make/break translation, modifiers, and PS/2 mouse packets |
| `block_tests` | Generic block device abstraction, sector read/write parity, multi-block I/O, bounds rejection, and read-only checks |
| `usb_tests` | Standard USB packed descriptor structures, setup packets, and host controller discovery models |
| `hardware_report` | Formatted structured hardware diagnostic report querying real CPUID, memory, ACPI, PCI, input, and storage |
| `storage_tests` | Storage coordinator, sector bounds checking, MBR/GPT table parsing, CRC32, and block cache read/write-back/sync |
| `zerofs_tests` | On-disk ZEROFS formatting, VFS mount at `/data`, multi-block file I/O, directory creation, unlinking, and sync |
| `net_tests` | `net_buf` zero-copy management, Ethernet framing, ARP caching, RFC 791 IPv4 checksum, ICMP echo, UDP, DHCP, DNS, TCP, and sockets |
| `network_report` | Formatted structured network diagnostic report detailing interfaces, MAC, IP, counters, ARP cache, and active sockets |
| `desktop_tests` | Linear framebuffer double buffering, damage tracking, 2D vector primitives, font metrics, cursor overlay, window manager, compositor, shell, login, native apps |
| `desktop_report` | Formatted structured desktop diagnostic report detailing graphics mode, double buffering, font matrix, window manager, shell, and applications |
| `ecosystem_tests` | .zapp binary package parsing, traversal defenses, checksums, ZPM package manager, application config, toast notifications, and application registry |
| `ecosystem_report` | Formatted structured ecosystem diagnostic report detailing package format, ZPM database, Store catalog, and registered applications |
| `security_tests` | Capability authorization, attenuation, revocation, process sandboxing, POSIX DAC, SHA-256 KATs, firewall filtering, and audit logging |
| `security_report` | Formatted structured security diagnostic report detailing enforcement state, capability telemetry, firewall rules, and audit stream |
| `intelligence_tests` | AI daemon lifecycle, provider abstraction, single-active-model memory checks, automation approval gates, engine context, and memory persistence |
| `intelligence_report` | Formatted structured intelligence diagnostic report detailing service state, daemon health, active provider, active model, and automation metrics |
| `ultimate_tests` | System profiles (NORMAL/GAMING/POWER_SAVER), scheduler low-latency boosts, GPU abstraction, audio abstraction, and AI resource coordination |
| `ultimate_report` | Formatted structured ultimate release diagnostic report detailing system profile, scheduler policy, GPU hardware, audio status, and 60 FPS telemetry |

## Running Tests

```bash
# Verify official 10-version manifest
python3 tests/check_versions.py

# Run standalone Hardware Foundation test suite
python3 tests/test_hardware.py

# Run standalone Storage & Networking test suite
python3 tests/test_storage_net.py

# Run standalone ZERO Desktop test suite
python3 tests/test_desktop.py

# Run standalone ZERO Ecosystem test suite
python3 tests/test_ecosystem.py

# Run standalone ZERO Security test suite
python3 tests/test_security.py

# Run standalone ZERO Intelligence test suite
python3 tests/test_intelligence.py

# Run standalone ZERO Ultimate test suite
python3 tests/test_ultimate.py

# Run performance benchmark suite
python3 tests/benchmark_suite.py

# Run complete boot harness (requires build toolchain)
python3 tests/run_tests.py
```
