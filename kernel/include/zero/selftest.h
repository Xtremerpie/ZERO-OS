/* ZERO KERNEL - boot-time self-tests and the QEMU test-exit device.
 *
 * Both are selected by the kernel command line (the `cmdline:` of the Limine entry):
 *   zero.selftest=<name>   bp (recoverable) | div0 | ud | gp | pf | df | panic | nested
 *                          every name above except bp ends in a panic BY DESIGN.
 *                          pmm        the 20 PMM tests (panics if any fails)
 *                          pmm_bench  PMM timing/fragmentation measurements (recoverable)
 *                          memmap_overlap  feeds the PMM an overlapping map: boot must FAIL SAFE (panic)
 *   zero.test_exit=1       (exactly "1") write the result to QEMU's isa-debug-exit device (port 0xF4):
 *                          0x10 -> QEMU exit status 33 (boot complete / pass),
 *                          0x11 -> QEMU exit status 35 (panic).
 * With neither option, nothing is triggered and port 0xF4 is never touched. */
#pragma once

#include <stdbool.h>
#include "zero/bootinfo.h"

/* Parse the command line; must run before selftest_run() and before any panic. */
void selftest_configure(const char *cmdline);

/* True if zero.selftest=<name> was given (checked by boot code for pre-PMM options). */
bool selftest_requested(const char *name);

/* Run the requested selftest, if any. Call after PMM init. */
void selftest_run(const struct boot_info *bi);

/* If zero.test_exit=1, signal QEMU and do not return. Otherwise returns. */
void test_device_exit(bool ok);
