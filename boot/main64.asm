section .bss
align 16
stack64_bottom:
    resb 4096 * 16 ; 64 KB reserved
stack64_top:

section .text
bits 64
global long_mode_start
extern kernel_main

long_mode_start:
    mov rsp, stack64_top ; set up stack for long mode
    mov rbp, rsp
    ; load null to all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    call kernel_main ; initialize kernel
    
.halt
    hlt
    jmp .halt