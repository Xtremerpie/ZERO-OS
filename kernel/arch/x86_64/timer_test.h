/* ZERO KERNEL - Timer & Interrupt self-test suite (Version 1.0.0 Core Foundation). */
#pragma once

#include <stdbool.h>

/* Runs all APIC and timer self-tests. Returns true if all pass. */
bool timer_run_selftests(void);
