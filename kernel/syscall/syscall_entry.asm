; ZERO KERNEL - System Call Entry and User-Mode Transition (Version 2.0.0 System Architecture).
;
; Implements:
; - enter_user_mode(entry_rip, user_rsp): drops from Ring 0 to Ring 3 via iretq.
; - syscall_entry: x86_64 fast syscall entry via MSR_LSTAR and return via sysretq.

bits 64

global enter_user_mode
global syscall_entry
global current_kernel_rsp

extern syscall_dispatch

section .bss
align 8
global user_rsp_scratch
user_rsp_scratch: resq 1
current_kernel_rsp: resq 1

section .text

enter_user_mode:
    ; RDI = uint64_t entry_rip
    ; RSI = uint64_t user_rsp

    ; Set segments
    mov ax, (0x28 | 3)      ; User DS (GDT_USER_DS | RPL3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Construct iretq frame
    push qword (0x28 | 3)   ; User SS
    push rsi                ; User RSP
    push qword 0x202        ; User RFLAGS (IF = 1)
    push qword (0x30 | 3)   ; User CS (GDT_USER_CS | RPL3)
    push rdi                ; User RIP
    iretq

syscall_entry:
    ; User mode executed `syscall`.
    ; RCX = user RIP, R11 = user RFLAGS, RSP = user stack.
    ; Interrupts disabled by MSR_SFMASK.

    ; 1. Switch to kernel stack
    mov [rel user_rsp_scratch], rsp
    mov rsp, [rel current_kernel_rsp]

    ; 2. Push user state
    push qword [rel user_rsp_scratch] ; User RSP
    push r11                          ; User RFLAGS
    push rcx                          ; User RIP

    ; Save callee-saved and caller registers
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdx
    push rsi
    push rdi

    ; Align stack to 16 bytes for System V ABI call
    mov rbp, rsp
    and rsp, -16

    ; Re-route arguments to syscall_dispatch(num, arg1, arg2, arg3, arg4, arg5):
    ; RAX = num
    ; RDI = arg1 -> RSI
    ; RSI = arg2 -> RDX
    ; RDX = arg3 -> RCX
    ; R10 = arg4 -> R8
    ; R8  = arg5 -> R9
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax

    call syscall_dispatch

    ; Restore stack from rbp
    mov rsp, rbp

    ; Restore registers
    pop rdi
    pop rsi
    pop rdx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Pop user RIP, RFLAGS, and restore user RSP
    pop rcx
    pop r11
    pop rsp

    ; Return to user mode (loads CS/SS from STAR MSR, RIP from RCX, RFLAGS from R11)
    sysretq

section .note.GNU-stack noalloc noexec nowrite progbits
