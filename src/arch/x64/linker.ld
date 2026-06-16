ENTRY(_start)

SECTIONS
{
    /* Multiboot expects the kernel to be loaded at 1MB or higher */
    . = 1M;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
        . = ALIGN(8);
        KEEP(build/kernel_sgvm.o(.data))
    }

    .bss : {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    }
}
