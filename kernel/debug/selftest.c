/* ZERO KERNEL - boot-time self-tests and QEMU test-exit device. See selftest.h. */

#include <stdbool.h>
#include <stdint.h>
#include "zero/selftest.h"
#include "zero/cpu.h"
#include "zero/idt.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"
#include "memory/pmm_test.h"
#include "memory/vmm_test.h"
#include "memory/heap_test.h"
#include "lib/spinlock_test.h"
#include "arch/x86_64/timer_test.h"

#define ISA_DEBUG_EXIT_PORT 0xF4

static char test_name[16];
static bool test_exit_enabled;
static bool console_only_enabled;

/* Finds `key` in a space-separated command line; copies its value into out. */
static bool cmdline_get(const char *cmdline, const char *key, char *out, size_t out_size)
{
    size_t klen = strlen(key);
    for (const char *p = cmdline; *p;) {
        while (*p == ' ')
            p++;
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            p += klen + 1;
            size_t i = 0;
            while (*p && *p != ' ' && i + 1 < out_size)
                out[i++] = *p++;
            out[i] = '\0';
            return true;
        }
        while (*p && *p != ' ')
            p++;
    }
    return false;
}

void selftest_configure(const char *cmdline)
{
    char val[4];
    test_name[0] = '\0';
    test_exit_enabled = false;
    console_only_enabled = false;
    if (!cmdline)
        return;
    cmdline_get(cmdline, "zero.selftest", test_name, sizeof test_name);
    /* Exactly "1": "10", "1x" etc. must not enable the QEMU exit device. */
    test_exit_enabled = cmdline_get(cmdline, "zero.test_exit", val, sizeof val) &&
                        val[0] == '1' && val[1] == '\0';
    console_only_enabled = cmdline_get(cmdline, "zero.console", val, sizeof val) &&
                           val[0] == '1' && val[1] == '\0';
}

bool selftest_console_only(void)
{
    return console_only_enabled;
}

void test_device_exit(bool ok)
{
    if (!test_exit_enabled)
        return;
    outb(ISA_DEBUG_EXIT_PORT, ok ? 0x10 : 0x11);
    cpu_halt_forever();
}

bool selftest_requested(const char *name)
{
    return test_name[0] != '\0' && strcmp(test_name, name) == 0;
}

void selftest_run(const struct boot_info *bi)
{
    if (test_name[0] == '\0')
        return;

    KLOG_WARN("selftest", "requested: %s", test_name);

    if (strcmp(test_name, "bp") == 0) {
        uint64_t before = exception_breakpoint_count();
        __asm__ volatile("int3");
        uint64_t after = exception_breakpoint_count();
        if (after == before + 1)
            KLOG_INFO("selftest", "bp: PASS (breakpoint taken, handled and resumed)");
        else
            kpanic("selftest bp: FAIL (count %lu -> %lu)", before, after);
    } else if (strcmp(test_name, "div0") == 0) {
        __asm__ volatile("xorl %%edx, %%edx\n\tmovl $1, %%eax\n\txorl %%ecx, %%ecx\n\tdivl %%ecx"
                         : : : "eax", "ecx", "edx", "cc");
    } else if (strcmp(test_name, "ud") == 0) {
        __asm__ volatile("ud2");
    } else if (strcmp(test_name, "gp") == 0) {
        /* Non-canonical address: always #GP. */
        volatile uint64_t *bad = (volatile uint64_t *)0x0000800000000000ull;
        (void)*bad;
    } else if (strcmp(test_name, "pf") == 0) {
        /* Low addresses are unmapped in the Limine base-revision >= 1 environment. */
        volatile uint64_t *bad = (volatile uint64_t *)0xDEAD0000ull;
        (void)*bad;
    } else if (strcmp(test_name, "df") == 0) {
        /* Point RSP at unmapped memory, then take an exception: the CPU cannot push the
         * frame (#PF), cannot deliver that either, and escalates to #DF on IST1. */
        __asm__ volatile("movq $0x1000, %%rsp\n\tint3" : : : "memory");
    } else if (strcmp(test_name, "panic") == 0) {
        kpanic("selftest panic: deliberate kernel panic");
    } else if (strcmp(test_name, "nested") == 0) {
        /* A fault INSIDE the panic path: the %s argument is a non-canonical pointer, so
         * printing the panic message itself raises #GP. Must produce a clear nested-panic
         * report and halt, never recurse or hang silently. */
        kpanic("selftest nested: message follows: %s", (const char *)0x0000800000000000ull);
    } else if (strcmp(test_name, "pmm") == 0) {
        unsigned failed = pmm_selftest_all(bi);
        if (failed)
            kpanic("selftest pmm: %u test(s) FAILED", failed);
    } else if (strcmp(test_name, "pmm_bench") == 0) {
        pmm_selftest_bench(bi);
    } else if (strcmp(test_name, "memmap_overlap") == 0) {
        /* handled before PMM init in kmain(); reaching here means the PMM accepted a corrupt map */
        kpanic("selftest memmap_overlap: corrupt memory map was ACCEPTED");
    } else if (strcmp(test_name, "vmm") == 0) {
        if (!vmm_run_selftests())
            kpanic("selftest vmm: FAILED");
    } else if (strcmp(test_name, "heap") == 0) {
        if (!heap_run_selftests())
            kpanic("selftest heap: FAILED");
    } else if (strcmp(test_name, "lock") == 0) {
        if (!spinlock_run_selftests())
            kpanic("selftest lock: FAILED");
    } else if (strcmp(test_name, "timer") == 0) {
        if (!timer_run_selftests())
            kpanic("selftest timer: FAILED");
    } else if (strcmp(test_name, "core") == 0) {
        if (!vmm_run_selftests())
            kpanic("selftest core (vmm): FAILED");
        if (!heap_run_selftests())
            kpanic("selftest core (heap): FAILED");
        if (!spinlock_run_selftests())
            kpanic("selftest core (lock): FAILED");
        if (!timer_run_selftests())
            kpanic("selftest core (timer): FAILED");
        KLOG_INFO("selftest", "ALL CORE FOUNDATION SELF-TESTS PASSED!");
    } else {
        KLOG_ERROR("selftest", "unknown selftest '%s' (ignored)", test_name);
    }
}
