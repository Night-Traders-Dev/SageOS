.global handle_ecall
handle_ecall:
    /* a7 = syscall number (Linux rv64 ABI)
     * a0-a5 = args
     * Return in a0
     */
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    addi s0, sp, 64

    /* Shift args for syscall_dispatch(num, a1, a2, a3, a4, a5) */
    mv a5, a4
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    mv a0, a7

    call syscall_dispatch

    ld ra, 56(sp)
    ld s0, 48(sp)
    addi sp, sp, 64
    sret
