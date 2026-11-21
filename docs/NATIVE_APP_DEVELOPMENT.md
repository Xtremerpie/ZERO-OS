# ZERO OS Native Application Development Guide

**Audience:** Software Engineers & Third-Party Developers  
**Platform:** ZERO OS 6.0.0+  
**Target:** Native C11 + ZERO SDK

---

## 1. Introduction

Developing native applications for ZERO OS gives you direct access to the microkernel's high-performance graphics compositor, 4KB page-buffered storage, BSD network sockets, and hardware drivers with zero intermediate translation overhead.

All applications run as isolated Ring 3 user processes linked against the ZERO SDK.

---

## 2. Development Workflow

```
Write C Code (using sdk/include/zero/*.h)
  ↓
Compile Object with clang (--target=x86_64-unknown-none-elf)
  ↓
Link Binary with ld.lld (-T sdk/linker_user.ld sdk/crt0.o ...)
  ↓
Package with tools/zapp_tool.py into .zapp container
  ↓
Install and Test via zpm or ZERO Store
```

---

## 3. Step-by-Step Tutorial

### Step 1: Create Application Directory
```bash
mkdir -p myapp/bin myapp/icons
```

### Step 2: Write Application Logic (`myapp/main.c`)
```c
#include "zero/app.h"
#include "zero/window.h"
#include "zero/draw.h"

static void my_paint(zero_surface_t *surf)
{
    zero_draw_fill_rect(surf, 0, 0, surf->width, surf->height, 0x121216);
    zero_draw_string(surf, 16, 24, "My Native ZERO Application", COLOR_ZERO_GOLD);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    zero_app_config_t cfg = {
        .title = "My App",
        .width = 400,
        .height = 300,
        .on_paint = my_paint
    };
    return zero_app_run(&cfg);
}
```

### Step 3: Compile and Link
```bash
clang -c main.c -I/path/to/sdk/include -ffreestanding -fno-stack-protector -o main.o
nasm -f elf64 sdk/crt0.asm -o crt0.o
ld.lld -T sdk/linker_user.ld crt0.o main.o -o myapp/bin/app
```

### Step 4: Write Package Manifest (`manifest.json`)
```json
{
  "app_id": "com.example.myapp",
  "version": "1.0.0",
  "name": "My Application",
  "description": "High performance native ZERO OS app",
  "category": "Utilities",
  "author": "Developer",
  "license": "MIT",
  "exec_path": "bin/app",
  "icon_path": "icons/app.png",
  "permissions": ["window", "storage"]
}
```

### Step 5: Package into `.zapp`
```bash
python tools/zapp_tool.py create --manifest manifest.json --dir myapp/ --out myapp-1.0.0.zapp
```
Now distribute `myapp-1.0.0.zapp` to users via the ZERO Store or install directly with `zpm install`.
