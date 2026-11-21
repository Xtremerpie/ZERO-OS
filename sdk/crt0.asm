; ZERO OS - User-Space Application Entry Point (Milestone 6.0.0 ZERO Ecosystem)
; Sets up stack frame, prepares arguments, calls main, and invokes sys_exit.

[bits 64]
global _start
extern main

section .text
_start:
    ; Terminate stack frame base
    xor rbp, rbp

    ; Extract argc, argv from initial stack
    pop rdi             ; rdi = argc
    mov rsi, rsp        ; rsi = argv

    ; Align stack to 16 bytes per System V AMD64 ABI
    and rsp, -16
    push rbp
    mov rbp, rsp

    ; Call main(argc, argv)
    call main

    ; Exit syscall (rax = return code)
    mov rdi, rax        ; arg1: exit code
    mov eax, 1          ; SYS_EXIT = 1
    syscall

.halt:
    hlt
    jmp .halt
