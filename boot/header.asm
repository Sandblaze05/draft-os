section .multiboot_header
header_start:
    dd 0xe85250d6 ; mutiboot 2
    
    dd 0 ; protected mode i386
    
    dd header_end - header_start
    ; checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; end
    dw 0 ; type
    dw 0 ; flag
    dd 8 ; size
header_end:
