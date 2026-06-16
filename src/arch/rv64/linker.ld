OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

    .text : {
        *(.text.entry)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
        . = ALIGN(8);
        _kernel_start = .;
        KEEP(build/kernel_sgvm.o(.data))
        _kernel_end = .;
        _kernel_size = _kernel_end - _kernel_start;
    }

    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }

    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.note.gnu.build-id)
        *(.comment)
    }
}
