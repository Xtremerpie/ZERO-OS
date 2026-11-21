/* ZERO KERNEL - APIC Timer Subsystem (Version 1.0.0 Core Foundation).
 *
 * Implements high-precision periodic timer ticks using the Local APIC.
 * Tracks kernel system ticks, millisecond uptime, and sleep intervals.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TIMER_DEFAULT_HZ 100u /* 100 Hz = 10 ms per tick */

/* Initialize the APIC timer with the specified tick frequency (e.g. 100 Hz). */
void timer_init(uint32_t frequency_hz);

/* Check if timer is initialized and ticking. */
bool timer_is_initialized(void);

/* Retrieve cumulative timer ticks since boot. */
uint64_t timer_get_ticks(void);

/* Retrieve system uptime in milliseconds. */
uint64_t timer_get_uptime_ms(void);

/* Busy-wait sleep for approximately ms milliseconds using timer ticks. */
void timer_sleep_ms(uint64_t ms);
