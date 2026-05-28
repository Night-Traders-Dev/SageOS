/* toolchain/newlib/libc/sys/sageos/crt0.rv64.S */
    .section .text
    .global _start
    .type   _start, @function

_start:
    /* Align stack to 16 bytes */
    andi    sp, sp, -16

    /* Clear BSS */
    la      a0, __bss_start
    la      a1, _end
    sub     a2, a1, a0
    li      a1, 0
    call    memset

    /* Load argc, argv from stack */
    lw      a0, 0(sp)             /* argc */
    addi    a1, sp, 8             /* argv */
    slli    t0, a0, 3
    add     a2, a1, t0
    addi    a2, a2, 8             /* envp */

    call    main

    /* exit */
    mv      a0, a0
    li      a7, 93               /* SYS_exit */
    ecall
    unimp                         /* unreachable */

    .size _start, .-_start
