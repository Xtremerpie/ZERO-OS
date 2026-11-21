# ZERO Software Development Kit (SDK) Guide

**SDK Version:** 1.0.0 (Milestone 6.0.0)  
**Target:** x86_64 Freestanding / Ring 3 User-Space  
**ABI Standard:** System V AMD64 ABI + ZERO Microkernel System Calls

---

## 1. Directory Structure

The ZERO SDK resides under the `sdk/` tree:

```
sdk/
├── crt0.asm                      # x86_64 user-space startup assembly
├── linker_user.ld                # Linker script placing code at 0x400000
├── include/
│   └── zero/
│       ├── syscall.h             # Ring 3 syscall wrappers (zero_exit, zero_write, ...)
│       ├── app.h                 # Application configuration & ZERO_APP_MAIN
│       ├── window.h              # Window creation and event dispatch
│       ├── draw.h                # 2D drawing routines & surface primitives
│       └── event.h               # Event structures (mouse, keyboard, quit)
└── templates/
    └── hello_world/
        └── main.c                # Canonical SDK demonstration application
```

---

## 2. Kernel System Call ABI

ZERO OS uses standard x86_64 `syscall` instructions for kernel transitions. Arguments are passed in registers:
- `RAX`: System Call Number
- `RDI`: Argument 1
- `RSI`: Argument 2
- `RDX`: Argument 3
- `R10`: Argument 4
- `R8`:  Argument 5
- `R9`:  Argument 6
- Return value in `RAX`.

### Core System Calls (`sdk/include/zero/syscall.h`):
- `SYS_EXIT (0x01)`: Terminate current process.
- `SYS_FORK (0x02)`: Fork current process.
- `SYS_READ (0x03)`: Read bytes from file descriptor.
- `SYS_WRITE (0x04)`: Write bytes to file descriptor.
- `SYS_OPEN (0x05)`: Open or create file.
- `SYS_CLOSE (0x06)`: Close file descriptor.
- `SYS_YIELD (0x07)`: Yield CPU timeslice.
- `SYS_SOCKET (0x0B)`: Create BSD socket.
- `SYS_BIND (0x0C)`: Bind socket to local port.
- `SYS_CONNECT (0x0E)`: Connect to remote host.
- `SYS_SEND (0x0F)`: Transmit data over socket.
- `SYS_RECV (0x10)`: Receive data from socket.

---

## 3. Creating a Hello World Application

```c
#include "zero/app.h"
#include "zero/window.h"
#include "zero/draw.h"

static void on_paint(zero_surface_t *surf)
{
    zero_draw_fill_rect(surf, 0, 0, surf->width, surf->height, COLOR_ZERO_PANEL);
    zero_draw_rect(surf, 0, 0, surf->width, surf->height, COLOR_ZERO_GOLD);
    zero_draw_string(surf, 20, 30, "Hello, ZERO OS!", COLOR_ZERO_GOLD);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    zero_app_config_t cfg = {
        .title = "Hello ZERO",
        .width = 320,
        .height = 200,
        .on_paint = on_paint
    };
    return zero_app_run(&cfg);
}
```
