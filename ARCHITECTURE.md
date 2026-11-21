# ZERO OS Architecture

Status key used across all docs: **[implemented+tested]** exists and is verified by test suites; **[designed]** a design only, nothing exists yet; **[later]** belongs to a later official version and is deliberately not started.

## 1. What exists (9.0.0 ZERO Ultimate - Full Operating System Complete)

```
UEFI firmware (OVMF in QEMU / Bare-Metal UEFI)
   └─ Limine 11.4.1 (third-party bootloader; NOT our kernel)          [implemented+tested]
        └─ ZERO kernel (original code, higher half 0xffffffff80000000)
             ├─ serial.c            COM1 115200 8N1 polled log channel[implemented+tested]
             ├─ fbcon.c             32-bpp framebuffer text + banner  [implemented+tested]
             ├─ log.c               kprintf / klog                    [implemented+tested]
             ├─ cpu.c               CPUID / CR / EFER / MSR report    [implemented+tested]
             ├─ gdt.c               GDT + TSS, Ring 0 & Ring 3, IST1  [implemented+tested]
             ├─ idt.c+isr.asm       vectors 0-47 (exceptions + APIC)  [implemented+tested]
             ├─ panic.c             register dump + nested panic      [implemented+tested]
             ├─ memory/memmap.c     memory-map validation/classify    [implemented+tested]
             ├─ memory/pmm.c        bitmap physical allocator (4KiB)  [implemented+tested]
             ├─ memory/vmm.c        4-level paging, W^X, CR3, HHDM    [implemented+tested]
             ├─ memory/heap.c       kernel heap (8 classes + pages)   [implemented+tested]
             ├─ lib/spinlock.c      atomic spinlocks, IRQ-save locks  [implemented+tested]
             ├─ arch/x86_64/apic.c  Local APIC & legacy PIC masking   [implemented+tested]
             ├─ arch/x86_64/timer.c APIC timer (100 Hz, vector 0x20)  [implemented+tested]
             ├─ drivers/driver.c    unified device registry & lifecycle[implemented+tested]
             ├─ hardware/acpi.c     ACPI RSDP, XSDT/RSDT, MADT, HPET  [implemented+tested]
             ├─ hardware/pci.c      PCI config space, bus enum, BARs  [implemented+tested]
             ├─ drivers/input.c     unified input queue, keycodes     [implemented+tested]
             ├─ drivers/ps2_kbd.c   PS/2 keyboard driver (IRQ 1)      [implemented+tested]
             ├─ drivers/ps2_mouse.c PS/2 mouse driver (IRQ 12)        [implemented+tested]
             ├─ drivers/block.c     generic block storage abstraction [implemented+tested]
             ├─ drivers/ramdisk.c   in-memory RAM disk ('ramdisk0')   [implemented+tested]
             ├─ drivers/virtio_blk.c VirtIO block driver ('vda')      [implemented+tested]
             ├─ drivers/usb.c       USB controller models & discovery [implemented+tested]
             ├─ hardware/hw_report.c structured hardware diagnostics  [implemented+tested]
             ├─ process/process.c   PCB, private PML4, fd_table, PID  [implemented+tested]
             ├─ process/thread.c    TCB, kernel & user stacks, states [implemented+tested]
             ├─ process/scheduler.c round-robin preemptive scheduler  [implemented+tested]
             ├─ process/context.asm context_switch, stack & CR3 swap  [implemented+tested]
             ├─ syscall/syscall.c   fast syscall dispatcher & validate[implemented+tested]
             ├─ syscall/entry.asm   syscall_entry & enter_user_mode   [implemented+tested]
             ├─ fs/vfs.c            vnode abstraction, mount, files   [implemented+tested]
             ├─ fs/ramfs.c          in-memory dynamic root filesystem [implemented+tested]
             ├─ ipc/ipc.c           ring-buffered inter-process queues[implemented+tested]
             ├─ loader/elf.c        ELF64 executable & stack loader   [implemented+tested]
             ├─ storage/storage.c   storage coordinator & stats       [implemented+tested]
             ├─ storage/partition.c MBR & GPT partition table parser  [implemented+tested]
             ├─ storage/bcache.c    4 KiB LRU block buffer cache      [implemented+tested]
             ├─ fs/zerofs.c         ZEROFS native persistent fs       [implemented+tested]
             ├─ net/net.c           network interface manager         [implemented+tested]
             ├─ net/net_buf.c       reference-counted packet buffers  [implemented+tested]
             ├─ drivers/net/e1000.c Intel 82540EM Gigabit NIC driver  [implemented+tested]
             ├─ net/ethernet.c      Ethernet II framing & dispatch    [implemented+tested]
             ├─ net/arp.c           RFC 826 ARP requests, reply, cache[implemented+tested]
             ├─ net/ipv4.c          RFC 791 IPv4 & Internet checksum  [implemented+tested]
             ├─ net/icmp.c          RFC 792 Echo Request/Reply responder[implemented+tested]
             ├─ net/udp.c           RFC 768 UDP datagrams & demux     [implemented+tested]
             ├─ net/dhcp.c          RFC 2131 DHCP client state machine[implemented+tested]
             ├─ net/dns.c           RFC 1035 DNS query/resolver       [implemented+tested]
             ├─ net/tcp.c           RFC 793 TCP state machine & window[implemented+tested]
             ├─ net/socket.c        BSD socket descriptor abstraction [implemented+tested]
             ├─ net/net_report.c    structured network diagnostic rep [implemented+tested]
             ├─ graphics/framebuffer.c double-buffer & damage blitter [implemented+tested]
             ├─ graphics/draw.c     2D vector primitives & alpha blend[implemented+tested]
             ├─ graphics/font.c     bitmap font matrix & icon glyphs  [implemented+tested]
             ├─ graphics/cursor.c   flicker-free vector pointer arrow [implemented+tested]
             ├─ desktop/window.c    window manager, z-order, hit-test [implemented+tested]
             ├─ desktop/compositor.c 7-layer scene compositor & damage[implemented+tested]
             ├─ desktop/shell.c     desktop shell, taskbar, launcher  [implemented+tested]
             ├─ desktop/login.c     graphical login & dev auth mode   [implemented+tested]
             ├─ desktop/app.c       native application registry (12)  [implemented+tested]
             ├─ desktop/terminal.c  interactive shell + zpm CLI       [implemented+tested]
             ├─ desktop/file_manager.c graphical VFS & associations   [implemented+tested]
             ├─ desktop/control_center.c settings (7 tabs + perms)    [implemented+tested]
             ├─ desktop/sysmon.c    real-time resource & proc monitor [implemented+tested]
             ├─ desktop/about.c     system specifications & branding  [implemented+tested]
             ├─ desktop/store.c     ZERO Store software center        [implemented+tested]
             ├─ desktop/calc.c      ZERO Calculator application       [implemented+tested]
             ├─ desktop/editor.c    ZERO Text Editor application      [implemented+tested]
             ├─ desktop/viewer.c    ZERO Image Viewer application     [implemented+tested]
             ├─ desktop/archiver.c  ZERO Archive Viewer application   [implemented+tested]
             ├─ desktop/netmon.c    ZERO Network Monitor application  [implemented+tested]
             ├─ desktop/security_center.c ZERO Security Center GUI    [implemented+tested]
             ├─ desktop/notification.c desktop toast overlays (240x52)[implemented+tested]
             ├─ apps/zapp.c         .zapp package format & traversal  [implemented+tested]
             ├─ apps/zpm.c          ZERO Package Manager subsystem    [implemented+tested]
             ├─ apps/config.c       persistent /data/config kv engine [implemented+tested]
             ├─ security/security.c central security coordinator      [implemented+tested]
             ├─ security/capability.c unforgeable capability handles  [implemented+tested]
             ├─ security/sandbox.c  process sandboxing & jail roots   [implemented+tested]
             ├─ security/permission.c POSIX DAC UID/GID mode bits     [implemented+tested]
             ├─ security/crypto/crypto.c FIPS SHA-256 + CSPRNG engine [implemented+tested]
             ├─ security/firewall.c stateful packet-filtering firewall[implemented+tested]
             ├─ security/audit.c    audit log ring buffer + VFS stream[implemented+tested]
             ├─ intelligence/ai_service.c  central AI coordinator          [implemented+tested]
             ├─ intelligence/ai_daemon.c   background assistant daemon     [implemented+tested]
             ├─ intelligence/ai_provider.c pluggable provider abstraction  [implemented+tested]
             ├─ intelligence/ai_model.c    model discovery & memory budget [implemented+tested]
             ├─ intelligence/ai_automation.c permission-gated broker       [implemented+tested]
             ├─ intelligence/ai_engine.c   assistant request engine        [implemented+tested]
             ├─ intelligence/ai_memory.c   persistent context in /data/ai/ [implemented+tested]
             ├─ intelligence/ai_audit.c    security audit integration      [implemented+tested]
             ├─ desktop/ai_assistant.c     ZERO AI Assistant 6-view GUI    [implemented+tested]
             ├─ gaming/gaming.c            ZERO Gaming Mode profile engine [implemented+tested]
             ├─ graphics/gpu.c             GPU hardware abstraction & blit [implemented+tested]
             ├─ drivers/audio.c            audio device & PCM ring buffer  [implemented+tested]
             ├─ desktop/demo_game.c        ZERO Strike 60 FPS native game  [implemented+tested]
             ├─ desktop/gaming_control.c   Gaming Control Center GUI (5vw) [implemented+tested]
             ├─ debug/selftest_desktop.c in-kernel desktop test suite [implemented+tested]
             ├─ debug/selftest_ecosystem.c in-kernel ecosystem suite  [implemented+tested]
             ├─ debug/selftest_security.c in-kernel security suite    [implemented+tested]
             ├─ debug/selftest_intelligence.c in-kernel AI test suite [implemented+tested]
             ├─ debug/selftest_ultimate.c in-kernel ultimate suite    [implemented+tested]
             └─ main.c              kernel bringup & subsystem tests  [implemented+tested]
```

All 10 official roadmap milestones (0.0.0 through 9.0.0) are fully implemented, tested, and validated. No official Milestone 10.0.0 exists.

## 2. Logical subsystem map (target architecture)

| Subsystem | Responsibility | First appears | State |
|-----------|----------------|---------------|-------|
| ZERO BOOT | UEFI + Limine handoff, memory map, boot diagnostics | 0.0.0 | [implemented+tested] |
| ZERO KERNEL | CPU init, IDT, PMM, VMM, heap, spinlocks, APIC, timer | 0.0.0 → 1.0.0 | [implemented+tested] |
| ZERO CORE | processes, threads, scheduler, IPC, VFS, ELF loader, Ring 3 | 2.0.0 | [implemented+tested] |
| ZERO HARDWARE | driver manager, ACPI, PCI, PS/2 HID, block I/O, VirtIO, USB | 3.0.0 | [implemented+tested] |
| ZERO STORAGE & NET | persistent ZEROFS filesystem, bcache, E1000 driver, TCP/IP stack | 4.0.0 | [implemented+tested] |
| ZERO DESKTOP | desktop shell, compositor, window manager, taskbar, apps | 5.0.0 | [implemented+tested] |
| ZERO CONTROL | settings center, versioned configuration rollback engine | 5.0.0 | [implemented+tested] |
| ZERO ECOSYSTEM | native apps, .zapp format, zpm manager, store, SDK | 6.0.0 | [implemented+tested] |
| ZERO SECURITY | capability security, sandboxing, firewall, crypto, audit | 7.0.0 | [implemented+tested] |
| ZERO INTELLIGENCE | AI assistant daemon, local model integration | 8.0.0 | [implemented+tested] |
| ZERO ULTIMATE | gaming mode, low-latency scheduler, GPU & audio abstraction | 9.0.0 | [implemented+tested] |

## 3. Boot flow (implemented)

1. Firmware loads `\EFI\BOOT\BOOTX64.EFI` (Limine) from the ESP.
2. Limine reads `limine.conf`, loads `boot():/boot/zero-kernel.elf`, switches to long mode, builds page tables (higher-half kernel mapping + HHDM), and jumps to the ELF entry `kmain`.
3. `kmain` initialises serial, checks the Limine base-revision handshake, draws the framebuffer banner, logs bootloader/CPU/memory information, loads GDT+TSS and IDT.
4. Initializes memory: PMM bitmap frame allocator, VMM kernel page tables with $W \oplus X$ enforcement, kernel heap, Local APIC, and 100 Hz periodic timer.
5. Initializes Hardware Foundation: Driver Manager, ACPI discovery, PCI enumeration, unified Input subsystem, Block device subsystem (`ramdisk0`, `vda`), USB host controller architecture discovery, and outputs structured hardware diagnostic report.
6. Initializes core architecture: Process manager (Kernel PID 0), Thread manager, Preemptive Scheduler, Fast System Call MSR interface, VFS, in-memory RAMFS mounted at `/`, and IPC mailboxes.
7. Initializes Storage & Networking Foundation: Central storage manager, block buffer cache (`bcache`), ZEROFS native filesystem mounted at `/data`, zero-copy network buffer pool (`net_buf`), Intel 82540EM (E1000) Gigabit NIC driver, protocol stack (Ethernet II, ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP), BSD socket API syscalls 11-17, and outputs structured network diagnostic report.
8. Initializes ZERO Desktop Foundation: Linear framebuffer double buffering, damage region tracking, unified mouse cursor overlay, Window Manager, scene Compositor, Desktop Shell, Graphical Login screen, and registers native applications (Terminal, Files, Control Center, System Monitor, About).
9. Executes all subsystem in-kernel self-tests (VMM, Heap, Spinlocks, Timer, ACPI, PCI, Input, Block, USB, Process, Scheduler, VFS, IPC, ELF64, Storage, ZEROFS, Network, Desktop, Ecosystem, Security, ZERO Intelligence, and ZERO Ultimate).
10. Outputs structured release diagnostic report (`ZERO OS 9.0.0 ZERO ULTIMATE DIAGNOSTIC REPORT`).
11. Yields control to the preemptive low-latency scheduler and idle thread.
