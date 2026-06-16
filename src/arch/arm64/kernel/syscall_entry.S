.align 11
.global exception_vectors
exception_vectors:
    /* --- Current EL with SP0 --- */
    .align 7
    b el0_sync_handler
    .align 7
    b .
    .align 7
    b .
    .align 7
    b .

    /* --- Current EL with SPx --- */
    .align 7
    b el0_sync_handler
    .align 7
    b .
    .align 7
    b .
    .align 7
    b .

    /* --- Lower EL using AArch64 --- */
    .align 7
    b el0_sync_handler
    .align 7
    b .
    .align 7
    b .
    .align 7
    b .

    /* --- Lower EL using AArch32 --- */
    .align 7
    b .
    .align 7
    b .
    .align 7
    b .
    .align 7
    b .

.global el0_sync_handler
el0_sync_handler:
    /* Save registers */
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    
    /* x8 = syscall number, x0-x5 = args */
    mov x5, x4
    mov x4, x3
    mov x3, x2
    mov x2, x1
    mov x1, x0
    mov x0, x8

    bl syscall_dispatch

    ldp x29, x30, [sp], #16
    eret
