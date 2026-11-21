/* ZERO KERNEL - Virtual Memory Manager self-test suite (Version 1.0.0 Core Foundation). */
#pragma once

#include <stdbool.h>

/* Runs all VMM unit and integration self-tests. Returns true if all pass. */
bool vmm_run_selftests(void);
