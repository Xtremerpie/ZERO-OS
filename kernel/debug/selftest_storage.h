/* ZERO KERNEL - Storage Subsystem Self-Test Interface (Milestone 4.0.0 Storage & Networking). */
#pragma once

#include <stdbool.h>

/* Execute storage architecture, partition parser, and block cache self-tests */
bool storage_run_selftests(void);
