/* ZERO KERNEL - Spinlock self-test suite (Version 1.0.0 Core Foundation). */
#pragma once

#include <stdbool.h>

/* Runs all spinlock and concurrency self-tests. Returns true if all pass. */
bool spinlock_run_selftests(void);
