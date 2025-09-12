ENTRY(start)

SECTIONS 
{
    . = 1M;

    .boot :
    {
        KEEP(*(.multiboot_header))
    }

    _kernel_start = .;

    .text ALIGN(4K):
    {
        *(.text*)
    }

    .rodata ALIGN(4K):
    {
        *(.rodata*)
    }

    .data ALIGN(4K):
    {
        *(.data*)
    }

    .bss ALIGN(4K):
    {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4K);
    _kernel_end = .;
}