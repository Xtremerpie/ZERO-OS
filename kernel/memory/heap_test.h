/* ZERO KERNEL - Heap self-test suite (Version 1.0.0 Core Foundation). */
#pragma once

#include <stdbool.h>

/* Runs all heap allocator self-tests. Returns true if all pass. */
bool heap_run_selftests(void);
