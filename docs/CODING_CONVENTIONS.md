# Coding Conventions

- C11 freestanding; builds clean with `-Wall -Wextra -Werror`. No libc; no SSE/FPU in kernel code.
- 4-space indent, K&R braces, functions `snake_case`, subsystem-prefixed (`serial_`, `fbcon_`, `idt_`).
- Public interface = a header in `kernel/include/zero/` with a comment stating contract and failure behaviour.
- Logging: `KLOG_*("subsystem", ...)`; never print without a subsystem tag.
- Every error path is handled or explicitly documented as fatal (`kpanic`).
- Assembly only where C cannot express it; NASM, Intel syntax, uniform frame layout documented beside the C struct.
- Third-party code only in `third_party/`, pinned and recorded in `THIRD_PARTY.md`.
- A feature is "done" only with a test in `tests/run_tests.py` and docs stating what is *not* done.
- Commits: one logical change; checkpoint (git commit) before architectural changes.
