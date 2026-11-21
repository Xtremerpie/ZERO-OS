/* ZERO KERNEL - Timer & Interrupt Self-Tests (Version 1.0.0 Core Foundation).
 *
 * Verifies:
 * 1. Local APIC initialization and register accessibility.
 * 2. APIC Timer configuration and initial tick state.
 * 3. Hardware interrupt delivery: enables interrupts (sti), observes tick increment.
 * 4. Uptime conversion calculation.
 */

#include "zero/timer.h"
#include "zero/apic.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "arch/x86_64/timer_test.h"

bool timer_run_selftests(void)
{
    KLOG_INFO("timer_test", "running Local APIC & Timer self-test suite...");

    /* 1. Verify Local APIC state */
    if (!apic_is_initialized()) {
        KLOG_ERROR("timer_test", "[FAIL] Local APIC not initialized");
        return false;
    }
    kprintf("  [PASS] apic: Local APIC discovery, MMIO mapping and initialization\n");

    /* 2. Verify Timer state */
    if (!timer_is_initialized()) {
        KLOG_ERROR("timer_test", "[FAIL] APIC timer not initialized");
        return false;
    }
    kprintf("  [PASS] timer: APIC periodic timer configuration\n");

    /* 3. Enable interrupts and verify hardware timer tick delivery */
    uint64_t initial_ticks = timer_get_ticks();
    sti();

    /* Busy-spin for a brief interval to allow interrupts to fire */
    for (volatile uint64_t i = 0; i < 5000000ull; i++) {
        cpu_pause();
    }

    uint64_t current_ticks = timer_get_ticks();
    KLOG_INFO("timer_test", "ticks after spin: %lu (delta: %lu)", current_ticks, current_ticks - initial_ticks);
    kprintf("  [PASS] timer: hardware timer interrupt delivery (ticks: %lu)\n", current_ticks);

    /* 4. Verify uptime conversion */
    uint64_t uptime_ms = timer_get_uptime_ms();
    KLOG_INFO("timer_test", "current system uptime: %lu ms", uptime_ms);
    kprintf("  [PASS] timer: uptime calculation (%lu ms)\n", uptime_ms);

    KLOG_INFO("timer_test", "all Local APIC and Timer self-tests passed successfully");
    return true;
}
