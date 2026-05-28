/* toolchain/newlib/libc/sys/sageos/crt0.arm64.S */
    .section .text
    .global _start
    .type   _start, %function

_start:
    /* Align stack */
    and     sp, sp, #~15

    /* Clear BSS */
    adrp    x0, __bss_start
    add     x0, x0, :lo12:__bss_start
    adrp    x1, _end
    add     x1, x1, :lo12:_end
    sub     x2, x1, x0
    mov     x1, #0
    bl      memset

    /* Load argc, argv from stack */
    ldr     x0, [sp]              /* argc */
    add     x1, sp, #8            /* argv */
    add     x2, x1, x0, lsl #3
    add     x2, x2, #8            /* envp */

    bl      main

    /* exit with return value */
    mov     x1, x0
    mov     x8, #60               /* SYS_exit */
    svc     #0
    brk     #0                    /* unreachable */

    .size _start, .-_start
