# Milestone 2.0.0 — System Architecture: Engineering Completion Report

**Official Version:** 2.0.0  
**Codename:** System Foundation (System Architecture)  
**Status:** IMPLEMENTED, TESTED, AND VALIDATED  
**Architecture:** x86_64 Higher-Half Preemptive Multitasking Kernel  
**Firmware:** UEFI / Limine 11.4.1 (Base Revision 6)  
**Target Hardware:** Intel Celeron J3355, 4 GB RAM, SSD, GPT  

---

## 1. Delivered Subsystems & Features

### A. Process Management Subsystem
- **Source:** `kernel/process/process.h`, `kernel/process/process.c`
- **Process Control Block (PCB):** Complete data structure managing process state (`NEW`, `READY`, `RUNNING`, `BLOCKED`, `TERMINATED`, `ZOMBIE`), PID, PPID, process name, exit status, thread counter, and execution accounting.
- **Private Virtual Address Spaces:**
  - Every non-kernel process receives a dedicated 4-level PML4 table allocated via `vmm_create_address_space`.
  - Kernel memory (higher half, `0xFFFF800000000000`+) is cloned and preserved; user lower half is strictly private.
- **File Descriptor Table:** Up to 32 open file descriptors (`MAX_FILE_DESCRIPTORS`) tracked per process.
- **Resource Cleanup:** Automated reclamation of open file handles, private PML4 tables, and IPC mailboxes on termination (`process_cleanup`).

### B. Thread Management Subsystem
- **Source:** `kernel/process/thread.h`, `kernel/process/thread.c`
- **Thread Control Block (TCB):** Manages thread state, priority, time slice quotas, and execution context.
- **Dual Execution Stacks:**
  - Dedicated 16 KiB kernel stack (`kstack_base`, `kstack_top`) allocated for every thread.
  - Ring 3 user stack (`user_rsp`, `user_rip`) for user-space tasks.
- **Separate Constructors:**
  - `thread_create_kernel`: Allocates kernel execution thread with synthesized callee-saved stack frame pointing to entry function.
  - `thread_create_user`: Prepares thread for user-mode execution via Ring 3 return.

### C. Preemptive Round-Robin Scheduler
- **Source:** `kernel/process/scheduler.h`, `kernel/process/scheduler.c`
- **APIC Timer Integration:** Timer interrupts delivered at 100 Hz trigger `scheduler_tick()`, decrementing time slices and enforcing preemptive context switches.
- **Queues & States:**
  - FIFO Run Queue (`ready_head`, `ready_tail`) tracking runnable threads.
  - Sleep Queue (`sleep_head`) handling timed delays with tick deadline wakeups.
  - Dedicated system Idle Thread (`idle_thread`) executing `hlt` loops when no user threads are runnable.
- **Statistics Accounting:** Live metrics for total context switches, preemptions, voluntary yields, and idle ticks.

### D. x86_64 Assembly Context Switching
- **Source:** `kernel/process/context.asm`
- **Register Context:** Preserves all SysV AMD64 callee-saved registers (`rbx, rbp, r12, r13, r14, r15, rsp, rflags`).
- **Stack & Address Space Swap:**
  - Atomically saves outgoing thread stack pointer and restores incoming thread stack pointer.
  - Automatically compares `CR3` physical base pointers; reloads `CR3` if switching across process address space boundaries.

### E. User Mode (Ring 3) Transition & GDT/TSS Architecture
- **Source:** `kernel/include/zero/gdt.h`, `kernel/arch/x86_64/gdt.c`, `kernel/syscall/syscall_entry.asm`
- **Segment Selectors:**
  - User Data Segment: `0x28` (DPL 3)
  - User Code Segment: `0x30` (DPL 3, 64-bit Long Mode)
- **Dynamic TSS RSP0 Updates:** `gdt_set_kernel_stack(rsp0)` called during context switch to ensure hardware interrupts and syscalls trap to the correct thread's kernel stack.
- **Atomic Ring 3 Entry:** `enter_user_mode` stub using `iretq` with user SS, RSP, RFLAGS (IF enabled), CS, and RIP.

### F. Fast System Call Interface (x86_64 syscall / sysret)
- **Source:** `kernel/syscall/syscall.h`, `kernel/syscall/syscall.c`, `kernel/syscall/syscall_entry.asm`
- **MSR Configuration:**
  - `MSR_STAR` (0xC0000081): Kernel CS `0x08`, User Base `0x20` (sysret generates SS=`0x28`, CS=`0x30`).
  - `MSR_LSTAR` (0xC0000082): Configured with `syscall_entry` address.
  - `MSR_SFMASK` (0xC0000084): Masks `IF` (0x200) on syscall entry.
  - `MSR_EFER` bit 0 (`SCE`): System Call Extensions enabled.
- **Security Validation:** `syscall_validate_user_ptr()` validates user pointers against 48-bit canonical user bounds (`< 0x0000800000000000`) with overflow prevention (`chk_add_u64`).
- **Dispatcher Subsystem:**
  - `SYS_EXIT` (1): Terminates process and halts thread.
  - `SYS_YIELD` (2): Voluntarily relinquishes CPU time slice.
  - `SYS_SLEEP` (3): Suspends thread for requested tick count.
  - `SYS_GETPID` (4): Returns active process PID.
  - `SYS_WRITE` (5): Writes to console (fd 1/2) or process open file descriptors.
  - `SYS_READ` (6): Reads from file descriptors.
  - `SYS_OPEN` (7): Opens VFS path and installs in process fd table.
  - `SYS_CLOSE` (8): Closes file descriptor.
  - `SYS_IPC_SEND` (9): Sends message to target process mailbox.
  - `SYS_IPC_RECV` (10): Receives message from process mailbox.

### G. Virtual File System (VFS) Abstraction
- **Source:** `kernel/fs/vfs.h`, `kernel/fs/vfs.c`
- Uniform vnode interface with `vnode_t`, `file_t`, and mount point table (`g_mounts`).
- Path traversal supporting multi-segment resolution (`/foo/bar`), prefix matching, and tokenization.
- Operations: `vfs_mount`, `vfs_open`, `vfs_close`, `vfs_read`, `vfs_write`, `vfs_seek`, `vfs_create`, `vfs_mkdir`, `vfs_unlink`.

### H. In-Memory RAMFS Filesystem
- **Source:** `kernel/fs/ramfs.h`, `kernel/fs/ramfs.c`
- In-memory hierarchical filesystem mounted at root (`/`).
- Dynamic buffer resizing for file data via `krealloc`.
- Directory indexing with child nodes array, lookup of `.` and `..`, nested subdirectories, and safe unlinking.

### I. Inter-Process Communication (IPC)
- **Source:** `kernel/ipc/ipc.h`, `kernel/ipc/ipc.c`
- Spinlock-protected circular FIFO mailbox queues per process (`IPC_QUEUE_CAP` = 16).
- Structured messages with sender/target PID, message type, and bounded payload (`IPC_MAX_PAYLOAD` = 128 bytes).
- Non-blocking and cooperative blocking receive semantics.

### J. ELF64 Executable Loader
- **Source:** `kernel/loader/elf.h`, `kernel/loader/elf.c`
- Full ELF64 identification and header validation (`0x7F 'E' 'L' 'F'`, 64-bit, little-endian, `EM_X86_64`, `ET_EXEC`/`ET_DYN`).
- Segment bounds checking and lower-half canonical address enforcement.
- `PT_LOAD` mapping with $W \oplus X$ permissions (code mapped `R-X`, data/BSS mapped `RW-NX`).
- Dedicated user stack allocation and mapping at `0x00007FFFF0000000`.

---

## 2. In-Kernel Self-Test Suites

All Milestone 2.0.0 subsystems include comprehensive self-tests executed at boot:
1. `selftest_process.c`: Validates PID 0, process creation, private PML4 allocation, kernel threads, user threads, process termination, resource cleanup, and count invariance.
2. `selftest_sched.c`: Validates scheduler active flag, idle thread initialization, current thread context, statistics tracking, and sleep queue deadline expiration ticks.
3. `selftest_vfs.c`: Validates root directory mounting, file creation, write and capacity expansion, seek, read-back verification, subdirectory creation, and unlink.
4. `selftest_ipc.c`: Validates PID rejection, unicast message delivery, sender PID verification, payload integrity, queue capacity limits, and mailbox clearing.
5. `selftest_elf.c`: Validates header corruption rejection, synthetic ELF64 validation, `PT_LOAD` page table mapping into a private address space, instruction payload verification, and user stack mapping.

---

## 3. Hardware Target Compliance (Intel Celeron J3355)

- **Memory Efficiency:** All process structures, thread stacks, and RAMFS buffers use minimal heap footprints with immediate reclamation on termination.
- **Low CPU Overhead:** Round-robin scheduler operates in $O(1)$ queue operations; context switches save only callee-saved registers without floating-point overhead.
- **Zero Undefined Behavior:** Built with freestanding C11 standards, `-Wall -Wextra -Werror`, no unaligned accesses, and strict memory safety canaries.
