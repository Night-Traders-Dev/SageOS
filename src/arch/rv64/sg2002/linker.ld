OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x80000000;
    _kernel_start = .;
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) *(COMMON) }
    
    . = ALIGN(16);
    _heap_start = .;
    . += 0x100000; /* 1MB heap */
    _heap_end = .;
    
    _kernel_end = .;
}
