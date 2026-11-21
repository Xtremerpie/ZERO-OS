/* ZERO KERNEL - PMM selftests and benchmark (kernel/debug/selftest_pmm.c). */
#pragma once

#include <stdbool.h>
#include "zero/bootinfo.h"

/* Always-on, cheap boot check (alloc/free/double-free/verify). Returns false on failure. */
bool pmm_boot_check(void);
/* The 20 directive tests. Prints [PASS]/[FAIL] per test; returns number of failures. */
unsigned pmm_selftest_all(const struct boot_info *bi);
/* Timing/fragmentation measurements (rdtsc cycles; see docs/PMM_DESIGN.md for caveats). */
void pmm_selftest_bench(const struct boot_info *bi);
