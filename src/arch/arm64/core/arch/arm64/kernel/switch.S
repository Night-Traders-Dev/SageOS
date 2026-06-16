.align 2
.global thread_switch
thread_switch:
    /* x0 = &prev->rsp, x1 = &next->rsp */
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    mov x2, sp
    str x2, [x0]
    ldr x2, [x1]
    mov sp, x2

    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16

    ret

.global thread_clone_and_switch
thread_clone_and_switch:
    /* x0 = parent, x1 = child, x2 = is_parent_offset */
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    /* Call C function to copy stack and calculate new RSP.
       It takes parent in x0, child in x1, current SP in x2, offset in x3. */
    mov x3, x2
    mov x2, sp
    bl copy_kernel_stack

    /* x0 now contains the child's RSP */
    mov sp, x0

    /* We are now on the child's stack! */
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16

    ret
