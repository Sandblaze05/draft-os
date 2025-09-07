global start
extern long_mode_start

section .text
bits 32
start:

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call zero_bss      ; clear bss section but never run after initializing stack.. will overwrite stack otherwise >:(
    mov esp, stack_top ; cpu uses ESP to store the stack pointer

    call setup_page_tables
    lgdt [gdt_ptr]                    ; now cpu knows where gdt is
    call enable_paging

    jmp CODE_SELECTOR:long_mode_start ; far jump to clear prefetch queue

    hlt

check_multiboot:
    cmp eax, 0x36d76289 ; check if eax registers holds this number
    jne .no_multiboot
    ret
.no_multiboot:
    mov al, "M" ; error code M for multiboot
    jmp error

check_cpuid:
    pushfd ; push flag register onto the stack
    pop eax ; pop into eax
    mov ecx, eax ; copy to ecx
    xor eax, 1 << 21 ; flip bit 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx ; cpu will flip the bit back to 0 if long mode is not supported
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "C" ; error code C for cpuid
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid ; implicitly takes eax as an arg and returns a number greater if extended processor info is supported
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29 ; check ln bit for long mode
    jz .no_long_mode
    
    ret
.no_long_mode:
    mov al, "L"
    jmp error

setup_page_tables:
    ; identity map the first 1GiB of memory using 2MiB pages
    ; each page table has 512 entries, each entry is 8 bytes
    ; page table L4 -> page table L3 -> page table L2 (2MiB pages)

    ; PML4[0] -> PDPT (L3)
    mov eax, page_table_l3
    or eax, 0b11 ; present, writable
    mov dword [page_table_l4 + 0*8], eax
    mov dword [page_table_l4 + 0*8 + 4], 0

    ; PDPT[0] -> PD (L2)
    mov eax, page_table_l2
    or eax, 0b11 ; present, writable
    mov dword [page_table_l3 + 0*8], eax
    mov dword [page_table_l3 + 0*8 + 4], 0

    mov ecx, 0 ; counter
.loop:

    mov eax, 0x200000 ; 2MiB
    mul ecx
    or eax, 0b10000011 ; present, writable, huge page
    mov [page_table_l2 + ecx*8], eax
    mov [page_table_l2 + ecx*8 + 4], edx

    inc ecx
    cmp ecx, 512
    jne .loop

    ret

enable_paging:
    ; enable PAE (Physical Adress Extension) for long mode (>4GB memory)
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; enable LME in EFER MSR
    mov ecx, 0xC0000080
    rdmsr               ; returns EDX:EAX
    or eax, (1 << 8)    ; set LME
    ; keep EDX as-is
    wrmsr

    ; load CR3 (PML4 physical base)
    mov eax, page_table_l4 ; control register 3 holds the physical address of the top-level page table
    mov cr3, eax

    ; enable paging via CR0.PG (paging bit)
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax
    ret

zero_bss:
    mov edi, page_table_l4
    mov ecx, (stack_top - page_table_l4) / 4
    xor eax, eax
.zb_loop:
    mov [edi], eax
    add edi, 4
    loop .zb_loop
    ret


error:
    mov dword [0xb8000], 0x4f524f45 ; print error code
    mov dword [0xb8004], 0x4f3a4f52 
    mov dword [0xb8008], 0x4f204f20
    mov byte [0xb800a], al
    hlt

section .bss
align 4096 ; each table is 4KB
page_table_l4:
    resb 4096
page_table_l3:
    resb 4096
page_table_l2:
    resb 4096
stack_bottom:
    resb 4096 * 4 ; 16 KB reserved
stack_top:

section .rodata
gdt64: ; global descriptor table
    dq 0x0000000000000000              ; NULL descriptor
    dq 0x00AF9B000000FFFF              ; 64-bit code descriptor (typical value) basically code segment
    dq 0x00AF93000000FFFF              ; data descriptor (unused in long mode) basically data segment
gdt64_end:

gdt_ptr:
    dw gdt64_end - gdt64 - 1 ; size of gdt - 1 aka table size
    dq gdt64                 ; address of gdt (base pointer)

CODE_SELECTOR equ 0x08