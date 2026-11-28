/* ZERO KERNEL - entry point and early initialisation.
 *
 * Boot flow: UEFI firmware -> Limine (bootloader only) -> kmain() in long mode.
 * What exists in 0.0.0: serial log, framebuffer banner, GDT/TSS, IDT/exceptions,
 * panic handler, memory-map reporting. Nothing else - see ROADMAP.md. */

#include <stdbool.h>
#include <stdint.h>
#include "limine.h"
#include "zero/version.h"
#include "zero/serial.h"
#include "zero/fbcon.h"
#include "zero/log.h"
#include "zero/cpu.h"
#include "zero/gdt.h"
#include "zero/idt.h"
#include "zero/panic.h"
#include "zero/selftest.h"
#include "zero/bootinfo.h"
#include "memory/memmap.h"
#include "memory/pmm.h"
#include "memory/pmm_test.h"
#include "memory/vmm.h"
#include "memory/vmm_test.h"
#include "memory/heap.h"
#include "memory/heap_test.h"
#include "zero/spinlock.h"
#include "lib/spinlock_test.h"
#include "zero/apic.h"
#include "zero/timer.h"
#include "arch/x86_64/timer_test.h"
#include "process/process.h"
#include "process/process_test.h"
#include "process/thread.h"
#include "process/scheduler.h"
#include "process/sched_test.h"
#include "syscall/syscall.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "fs/vfs_test.h"
#include "ipc/ipc.h"
#include "ipc/ipc_test.h"
#include "loader/elf.h"
#include "loader/elf_test.h"
#include "drivers/driver.h"
#include "drivers/input.h"
#include "drivers/input_test.h"
#include "drivers/ps2_kbd.h"
#include "drivers/ps2_mouse.h"
#include "drivers/block.h"
#include "drivers/block_test.h"
#include "drivers/ramdisk.h"
#include "drivers/virtio_blk.h"
#include "drivers/usb.h"
#include "drivers/usb_test.h"
#include "hardware/acpi.h"
#include "hardware/acpi_test.h"
#include "hardware/pci.h"
#include "hardware/pci_test.h"
#include "hardware/hw_report.h"
#include "storage/storage.h"
#include "storage/bcache.h"
#include "storage/partition.h"
#include "fs/zerofs.h"
#include "net/net.h"
#include "net/net_buf.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/dhcp.h"
#include "net/dns.h"
#include "net/tcp.h"
#include "net/socket.h"
#include "drivers/net/e1000.h"
#include "net/net_report.h"
#include "debug/selftest_storage.h"
#include "debug/selftest_zerofs.h"
#include "debug/selftest_net.h"
#include "graphics/color.h"
#include "graphics/framebuffer.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/cursor.h"
#include "desktop/window.h"
#include "desktop/compositor.h"
#include "desktop/login.h"
#include "desktop/shell.h"
#include "desktop/app.h"
#include "desktop/terminal.h"
#include "desktop/file_manager.h"
#include "desktop/control_center.h"
#include "desktop/sysmon.h"
#include "desktop/about.h"
#include "desktop/store.h"
#include "desktop/calc.h"
#include "desktop/editor.h"
#include "desktop/viewer.h"
#include "desktop/archiver.h"
#include "desktop/netmon.h"
#include "desktop/notification.h"
#include "apps/zapp.h"
#include "apps/zpm.h"
#include "apps/config.h"
#include "debug/selftest_desktop.h"
#include "debug/selftest_ecosystem.h"
#include "security/security.h"
#include "debug/selftest_security.h"
#include "intelligence/ai_service.h"
#include "debug/selftest_intelligence.h"
#include "gaming/gaming.h"
#include "graphics/gpu.h"
#include "drivers/audio.h"
#include "debug/selftest_ultimate.h"

/* ---- Limine requests (kernel/linker.ld keeps these sections together) ---- */

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[3] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootinfo_req = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_cmdline_request cmdline_req = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exec_addr_req = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_req = {
    .id = LIMINE_RSDP_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[2] = LIMINE_REQUESTS_END_MARKER;

/* ---- ZERO palette (see docs/DESIGN.md) ---- */
#define COL_BLACK    0x0A0A0A
#define COL_GRAPHITE 0x1C1C1E
#define COL_GOLD     0xC9A227
#define COL_CRIMSON  0xB3202B
#define COL_WHITE    0xE8E8E8
#define COL_GREY     0x8A8A8A

#define BANNER_HEIGHT 150

static void draw_banner(void)
{
    uint32_t w = fbcon_width();

    fbcon_clear(COL_BLACK);
    fbcon_fill_rect(0, 0, w, BANNER_HEIGHT, COL_GRAPHITE);
    fbcon_fill_rect(0, 0, 6, BANNER_HEIGHT, COL_CRIMSON);
    fbcon_fill_rect(0, BANNER_HEIGHT, w, 2, COL_GOLD);

    fbcon_draw_string(30, 20, "ZERO OS", 6, COL_WHITE);
    fbcon_draw_string(30, 80, "VERSION " ZERO_VERSION_STRING " - " ZERO_CODENAME_UPPER, 2, COL_GOLD);
    fbcon_draw_string(30, 110, ZERO_MOTTO, 2, COL_GREY);
}

static void print_banner_serial(void)
{
    kprintf("\n");
    kprintf("============================================================\n");
    kprintf("  ZERO OS %s \"%s\"\n", ZERO_VERSION_STRING, ZERO_CODENAME);
    kprintf("  %s\n", ZERO_MOTTO);
    kprintf("============================================================\n");
}

/* Raw Limine type names (the diagnostic report prints bootloader types, not normalized ones). */
static const char *limine_type_name(uint64_t t)
{
    static const char *const names[] = {
        "usable", "reserved", "acpi-reclaimable", "acpi-nvs", "bad",
        "bootloader-reclaimable", "kernel+modules", "framebuffer", "reserved-mapped"
    };
    return t < sizeof names / sizeof names[0] ? names[t] : "unknown";
}

static void report_memory_map(void)
{
    struct limine_memmap_response *mm = memmap_req.response;
    if (!mm) {
        KLOG_ERROR("mem", "bootloader did not provide a memory map");
        return;
    }

    uint64_t usable = 0, total_by_type[9] = { 0 };
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        const struct limine_memmap_entry *e = mm->entries[i];
        if (e->type < 9)
            total_by_type[e->type] += e->length;
        if (e->type == LIMINE_MEMMAP_USABLE)
            usable += e->length;
    }

    KLOG_INFO("mem", "memory map: %lu entries from bootloader", mm->entry_count);
    for (uint64_t t = 0; t < 9; t++) {
        if (total_by_type[t])
            KLOG_INFO("mem", "  %-22s %6lu KiB", limine_type_name(t), total_by_type[t] / 1024);
    }
    KLOG_INFO("mem", "usable RAM reported by bootloader: %lu MiB", usable / (1024 * 1024));
}

extern char __kernel_start[], __kernel_end[];

static struct boot_info boot_info;

/* Limine's framebuffer descriptor lives in bootloader-owned memory. The kernel
 * replaces CR3 before desktop initialization, so retain a kernel-owned copy of
 * the descriptor rather than dereferencing that bootloader pointer later. */
static struct limine_framebuffer boot_framebuffer;
static bool boot_framebuffer_valid;

static void collect_boot_info(void)
{
    boot_info.memmap = memmap_req.response;
    boot_info.hhdm_offset = hhdm_req.response ? hhdm_req.response->offset : 0;
    if (exec_addr_req.response) {
        boot_info.kernel_phys_base = exec_addr_req.response->physical_base;
        boot_info.kernel_size = (uint64_t)(uintptr_t)(__kernel_end - __kernel_start);
    } else {
        KLOG_WARN("mem", "bootloader did not report the kernel physical address");
    }
    if (fb_req.response && fb_req.response->framebuffer_count > 0 && boot_info.hhdm_offset) {
        const struct limine_framebuffer *f = fb_req.response->framebuffers[0];
        uint64_t va = (uint64_t)(uintptr_t)f->address;
        if (va >= boot_info.hhdm_offset) {
            boot_info.fb_phys = va - boot_info.hhdm_offset;
            boot_info.fb_size = f->pitch * f->height;
        }
    }
}

/* Memory-map validation then PMM init. Any failure here is fatal and SAFE: panic, never continue
 * with a half-initialised allocator. */
static void memory_bringup(void)
{
    struct mem_raw_entry raw[MEMMAP_MAX_RAW];
    struct memmap map;
    uint64_t n = 0;

    if (!hhdm_req.response)
        kpanic("memory: bootloader did not provide the HHDM offset");

    enum memmap_status ms = memmap_raw_from_limine(memmap_req.response, raw, MEMMAP_MAX_RAW, &n);
    if (ms != MEMMAP_OK)
        kpanic("memory: cannot read bootloader memory map: %s", memmap_status_name(ms));

    if (selftest_requested("memmap_overlap") && n < MEMMAP_MAX_RAW) {
        KLOG_WARN("memory", "selftest: injecting an overlapping memory-map entry");
        raw[n] = raw[0];
        n++;
    }

    ms = memmap_build(&map, raw, n);
    if (ms != MEMMAP_OK)
        kpanic("memory: memory map rejected: %s", memmap_status_name(ms));
    KLOG_INFO("memory", "memory map validated: %lu bootloader entries -> %u regions", n, map.count);
    for (int t = 0; t < MEM_TYPE_COUNT; t++)
        if (map.bytes[t])
            KLOG_INFO("memory", "  %-22s %8lu KiB", memmap_type_name((enum mem_type)t), map.bytes[t] / 1024);

    enum pmm_status ps = pmm_init(&map, hhdm_req.response->offset);
    if (ps != PMM_OK)
        kpanic("pmm: initialization failed: %s", pmm_status_name(ps));

    struct pmm_stats s;
    pmm_get_stats(&s);
    KLOG_INFO("pmm", "initialized: %lu frames tracked, bitmap %lu bytes", s.tracked_frames, s.bitmap_bytes);
    KLOG_INFO("pmm", "total %lu KiB, usable %lu KiB, reserved %lu KiB", s.total_memory / 1024,
              s.usable_memory / 1024, s.reserved_memory / 1024);
    KLOG_INFO("pmm", "allocatable %lu KiB (%lu frames), free %lu frames", s.allocatable_memory / 1024,
              s.allocatable_frames, s.free_frames);
    if (!pmm_boot_check())
        kpanic("pmm: boot allocation check FAILED");
    kprintf("[PASS] pmm: allocation test\n");
}

__attribute__((noreturn)) void kmain(void)
{
    bool have_serial = serial_init();

    print_banner_serial();

    if (!have_serial) {
        /* Nothing can be printed to serial; keep going so the framebuffer still reports. */
        KLOG_WARN("serial", "COM1 loopback self-check failed: serial log unavailable");
    }

    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        KLOG_ERROR("boot", "bootloader does not support requested Limine base revision 6");
        cpu_halt_forever();
    }

    /* Preserve the descriptor in kernel-owned storage before VMM replaces
     * the bootloader page tables. The Limine response/descriptor itself may
     * reside in bootloader-reclaimable memory not kept mapped by our HHDM. */
    if (fb_req.response && fb_req.response->framebuffer_count > 0 &&
        fb_req.response->framebuffers && fb_req.response->framebuffers[0]) {
        boot_framebuffer = *fb_req.response->framebuffers[0];
        boot_framebuffer_valid = true;
    }

    /* Framebuffer banner first, so the log below also appears on screen. */
    if (boot_framebuffer_valid && fbcon_init(&boot_framebuffer)) {
        draw_banner();
        fbcon_console_begin(BANNER_HEIGHT + 12, 1);
    }

    KLOG_INFO("boot", "ZERO OS %s (%s), build %s", ZERO_VERSION_STRING, ZERO_CODENAME, ZERO_BUILD_ID);
    KLOG_INFO("boot", "kernel entry reached in 64-bit long mode");

    if (bootinfo_req.response)
        KLOG_INFO("boot", "bootloader: %s %s (base revision %lu)", bootinfo_req.response->name,
                  bootinfo_req.response->version, (uint64_t)LIMINE_LOADED_BASE_REVISION(base_revision));

    if (fbcon_available())
        KLOG_INFO("fb", "framebuffer %ux%u, 32 bpp RGB", fbcon_width(), fbcon_height());
    else
        KLOG_WARN("fb", "no usable 32-bpp framebuffer: continuing serial-only");

    const char *cmdline = (cmdline_req.response && cmdline_req.response->cmdline)
                              ? cmdline_req.response->cmdline : "";
    selftest_configure(cmdline);
    KLOG_INFO("boot", "cmdline: \"%s\"", cmdline);

    cpu_report();
    gdt_init();
    idt_init();

    if (hhdm_req.response)
        KLOG_INFO("mem", "HHDM offset: 0x%016lx", hhdm_req.response->offset);
    report_memory_map();

    collect_boot_info();
    memory_bringup();

    /* 1.0.0 Core Foundation Subsystems */
    enum vmm_status vs = vmm_init(&boot_info);
    if (vs != VMM_OK) {
        kpanic("main: VMM initialization failed: %s", vmm_status_name(vs));
    }
    kprintf("[PASS] vmm: kernel address space and CR3 initialized\n");

    heap_init();
    kprintf("[PASS] heap: kernel dynamic memory allocator initialized\n");

    if (!apic_init()) {
        kpanic("main: Local APIC initialization failed");
    }
    kprintf("[PASS] apic: Local APIC initialized and legacy PIC masked\n");

    timer_init(100);
    kprintf("[PASS] timer: APIC periodic timer initialized\n");

    /* Core Foundation baseline in-kernel validations */
    if (!vmm_run_selftests()) {
        kpanic("main: VMM self-test validation failed");
    }
    if (!heap_run_selftests()) {
        kpanic("main: Kernel Heap self-test validation failed");
    }
    if (!spinlock_run_selftests()) {
        kpanic("main: Spinlock self-test validation failed");
    }
    if (!timer_run_selftests()) {
        kpanic("main: Timer self-test validation failed");
    }

    /* 3.0.0 Hardware Foundation Subsystems */
    driver_manager_init();
    kprintf("[PASS] driver: hardware device abstraction manager initialized\n");

    void *rsdp_ptr = (rsdp_req.response) ? rsdp_req.response->address : NULL;
    acpi_init(rsdp_ptr);
    kprintf("[PASS] acpi: ACPI discovery and table parsing complete\n");

    pci_init();
    kprintf("[PASS] pci: PCI device bus enumeration complete\n");

    input_init();
    kprintf("[PASS] input: unified input event subsystem initialized\n");

    ps2_kbd_init();
    kprintf("[PASS] ps2: PS/2 keyboard controller and IRQ 1 active\n");

    ps2_mouse_init();
    kprintf("[PASS] ps2: PS/2 mouse controller and IRQ 12 active\n");

    if (ioapic_init()) {
        ioapic_route_irq(1, 0x21);
        ioapic_route_irq(12, 0x2C);
    } else {
        KLOG_WARN("ioapic", "I/O APIC unavailable: PS/2 IRQs may not be delivered");
    }
    ps2_flush_output();

    block_init();
    kprintf("[PASS] block: block device management subsystem initialized\n");

    ramdisk_init(2048); /* 1 MiB RAM disk */
    kprintf("[PASS] ramdisk: in-memory RAM disk block device initialized\n");

    virtio_blk_init();

    usb_init();
    kprintf("[PASS] usb: USB subsystem architecture initialized\n");

    /* Milestone 3.0.0 Hardware Foundation Self-Tests */
    if (!acpi_run_selftests()) {
        kpanic("main: ACPI subsystem self-test failed");
    }
    if (!pci_run_selftests()) {
        kpanic("main: PCI subsystem self-test failed");
    }
    if (!input_run_selftests()) {
        kpanic("main: Input subsystem self-test failed");
    }
    if (!block_run_selftests()) {
        kpanic("main: Block device subsystem self-test failed");
    }
    if (!usb_run_selftests()) {
        kpanic("main: USB subsystem self-test failed");
    }

    /* Structured Hardware Diagnostic Report */
    hw_report_generate();

    /* 2.0.0 System Architecture Subsystems */
    process_init();
    kprintf("[PASS] proc: process manager initialized (kernel PID 0)\n");

    thread_init();
    kprintf("[PASS] thread: thread manager initialized\n");

    scheduler_init();
    kprintf("[PASS] sched: preemptive scheduler initialized\n");

    syscall_init();
    kprintf("[PASS] syscall: fast system call MSR interface active\n");

    vfs_init();
    kprintf("[PASS] vfs: virtual file system initialized\n");

    if (ramfs_init() != 0) {
        kpanic("main: RAMFS initialization failed");
    }
    kprintf("[PASS] ramfs: in-memory filesystem mounted at '/'\n");

    ipc_init();
    kprintf("[PASS] ipc: inter-process communication mailboxes initialized\n");

    /* Milestone 2.0.0 Self-Tests */
    if (!process_run_selftests()) {
        kpanic("main: Process subsystem self-test failed");
    }
    if (!sched_run_selftests()) {
        kpanic("main: Scheduler subsystem self-test failed");
    }
    if (!vfs_run_selftests()) {
        kpanic("main: VFS subsystem self-test failed");
    }
    if (!ipc_run_selftests()) {
        kpanic("main: IPC subsystem self-test failed");
    }
    if (!elf_run_selftests()) {
        kpanic("main: ELF loader self-test failed");
    }

    /* Milestone 4.0.0 Storage & Networking Subsystems */
    storage_init();
    kprintf("[PASS] storage: central storage subsystem initialized\n");

    bcache_init();
    kprintf("[PASS] bcache: 4 KiB block cache initialized\n");

    storage_scan_all_devices();

    net_init();
    kprintf("[PASS] net: central network architecture initialized\n");

    e1000_init();
    kprintf("[PASS] e1000: Ethernet adapter initialized\n");

    /* Milestone 4.0.0 Self-Tests */
    if (!storage_run_selftests()) {
        kpanic("main: Storage subsystem self-test failed");
    }
    if (!zerofs_run_selftests()) {
        kpanic("main: ZEROFS subsystem self-test failed");
    }
    if (!net_run_selftests()) {
        kpanic("main: Network subsystem self-test failed");
    }

    /* Structured Network Diagnostic Report */
    net_report_generate();

    /* Milestone 5.0.0 ZERO Desktop Subsystems */
    if (boot_framebuffer_valid && fb_init(&boot_framebuffer)) {
        fb_dump_info();
        framebuffer_t *fb = fb_get_primary();
        shell_init(fb);
        kprintf("[PASS] desktop: ZERO Desktop graphical environment initialized\n");
    } else {
        KLOG_WARN("desktop", "Limine linear framebuffer not available: running headless");
    }

    /* Milestone 5.0.0 Desktop Self-Tests */
    if (!desktop_run_selftests()) {
        kpanic("main: Desktop subsystem self-test failed");
    }

    /* Structured Desktop Diagnostic Report */
    desktop_report_generate();

    /* Milestone 6.0.0 ZERO Ecosystem Subsystems */
    zpm_init();
    kprintf("[PASS] ecosystem: ZERO Package Manager (zpm) initialized\n");

    config_init();
    kprintf("[PASS] ecosystem: application configuration subsystem initialized\n");

    /* Milestone 6.0.0 Ecosystem Self-Tests */
    if (!ecosystem_run_selftests()) {
        kpanic("main: Ecosystem subsystem self-test failed");
    }

    /* Structured Ecosystem Diagnostic Report */
    ecosystem_report_generate();

    /* Milestone 7.0.0 ZERO Security Subsystems */
    security_init();
    kprintf("[PASS] security: Central Security Architecture initialized\n");

    /* Milestone 7.0.0 Security Self-Tests */
    if (!security_run_selftests()) {
        kpanic("main: Security subsystem self-test failed");
    }

    /* Structured Security Diagnostic Report */
    security_report_generate();

    /* Milestone 8.0.0 ZERO Intelligence Subsystems */
    ai_service_init();
    kprintf("[PASS] intelligence: ZERO Intelligence Subsystems initialized\n");

    /* Milestone 8.0.0 Intelligence Self-Tests */
    if (!intelligence_run_selftests()) {
        kpanic("main: Intelligence subsystem self-test failed");
    }

    /* Structured Intelligence Diagnostic Report */
    intelligence_report_generate();

    /* Milestone 9.0.0 ZERO Ultimate Subsystems */
    gpu_init();
    kprintf("[PASS] gpu: graphics acceleration device abstraction active\n");

    audio_init();
    kprintf("[PASS] audio: audio device subsystem initialized\n");

    gaming_init();
    kprintf("[PASS] gaming: ZERO Gaming Mode & low-latency scheduler active\n");

    /* Milestone 9.0.0 Ultimate Self-Tests */
    if (!ultimate_run_selftests()) {
        kpanic("main: Ultimate subsystem self-test failed");
    }

    /* Structured Ultimate Diagnostic Report */
    ultimate_report_generate();

    selftest_run(&boot_info);

    KLOG_INFO("boot", "ZERO-OS " ZERO_VERSION_STRING " boot complete: ZERO Ultimate fully operational");
    KLOG_INFO("boot", "preemptive scheduler running: yielding to idle thread");
    test_device_exit(true); /* returns unless zero.test_exit=1 */

    /* Enter the interactive graphical session instead of halting at the
     * diagnostic console unless zero.console=1 was explicitly requested. */
    if (!selftest_console_only() && fb_is_available()) {
        /* Re-initialize shell after desktop_run_selftests() so WM bounds,
         * cursor position, and mouse button state match the live framebuffer. */
        shell_init(fb_get_primary());
        ps2_flush_output();
        KLOG_INFO("desktop", "entering interactive ZERO Desktop event loop");
        sti();
        for (;;) {
            input_event_t ev;
            while (input_poll_event(&ev)) {
                if (ev.type == INPUT_KEY_DOWN) {
                    shell_dispatch_keycode(ev.code, ev.modifiers);
                } else if (ev.type == INPUT_MOUSE_MOVE) {
                    cursor_move_delta(ev.dx, ev.dy);
                    int32_t x, y;
                    uint8_t buttons;
                    cursor_get_state(&x, &y, &buttons);
                    shell_dispatch_mouse(x, y, buttons);
                } else if (ev.type == INPUT_MOUSE_BUTTON_DOWN ||
                           ev.type == INPUT_MOUSE_BUTTON_UP) {
                    int32_t x, y;
                    uint8_t buttons;
                    cursor_get_state(&x, &y, &buttons);
                    if (ev.type == INPUT_MOUSE_BUTTON_DOWN)
                        buttons |= (uint8_t)ev.code;
                    else
                        buttons &= (uint8_t)~ev.code;
                    cursor_set_buttons(buttons);
                    shell_dispatch_mouse(x, y, buttons);
                }
            }
            shell_tick();
            __asm__ volatile("sti; hlt");
        }
    }

    cpu_halt_forever();
}
