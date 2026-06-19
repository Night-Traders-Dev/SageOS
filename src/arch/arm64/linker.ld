OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x40200000;

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
