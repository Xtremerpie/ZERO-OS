/* ZERO KERNEL - ZEROFS Native Filesystem Self-Test Interface (Milestone 4.0.0 Storage & Networking). */
#pragma once

#include <stdbool.h>

/* Execute ZEROFS formatting, mounting, file I/O, directory creation, and unlink self-tests */
bool zerofs_run_selftests(void);
