/* ZERO KERNEL - APIC Timer Implementation (Version 1.0.0 Core Foundation). */

#include "zero/timer.h"
#include "zero/apic.h"
#include "zero/idt.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "process/scheduler.h"

static bool timer_initialized = false;
static volatile uint64_t system_ticks = 0;
static uint32_t timer_hz = TIMER_DEFAULT_HZ;

static void timer_irq_handler(struct interrupt_frame *frame)
{
    (void)frame;
    system_ticks++;

    /* Acknowledge interrupt to Local APIC */
    apic_eoi();

    /* Preemptive scheduler tick */
    scheduler_tick();
}

void timer_init(uint32_t frequency_hz)
{
    if (!apic_is_initialized()) {
        KLOG_ERROR("timer", "cannot initialize timer: Local APIC is not initialized");
        return;
    }

    if (frequency_hz == 0)
        frequency_hz = TIMER_DEFAULT_HZ;
    timer_hz = frequency_hz;

    /* Register timer ISR on vector 0x20 */
    idt_register_handler(APIC_TIMER_VECTOR, timer_irq_handler);

    /* Set divide-by-16 in Timer Divide Configuration Register */
    apic_write(APIC_REG_TIMER_DIV, 0x03);

    /* Set LVT Timer to Periodic Mode (bit 17) on vector 0x20 */
    apic_write(APIC_REG_LVT_TIMER, (1u << 17) | APIC_TIMER_VECTOR);

    /* Initial count for periodic firing (~100 Hz calibration) */
    uint32_t init_count = 10000000u / timer_hz;
    apic_write(APIC_REG_TIMER_INITCNT, init_count);

    timer_initialized = true;
    KLOG_INFO("timer", "APIC periodic timer configured at %u Hz (vector 0x%02x, init_count: %u)",
              timer_hz, APIC_TIMER_VECTOR, init_count);
}

bool timer_is_initialized(void)
{
    return timer_initialized;
}

uint64_t timer_get_ticks(void)
{
    return system_ticks;
}

uint64_t timer_get_uptime_ms(void)
{
    if (timer_hz == 0)
        return 0;
    return (system_ticks * 1000ull) / timer_hz;
}

void timer_sleep_ms(uint64_t ms)
{
    uint64_t start_ticks = system_ticks;
    uint64_t needed_ticks = (ms * timer_hz) / 1000ull;
    if (needed_ticks == 0)
        needed_ticks = 1;

    while ((system_ticks - start_ticks) < needed_ticks) {
        cpu_pause();
    }
}
