.align 2
.global thread_switch
thread_switch:
    /* a0 = &prev->rsp, a1 = &next->rsp */
    addi sp, sp, -112
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    sd s7, 40(sp)
    sd s8, 32(sp)
    sd s9, 24(sp)
    sd s10, 16(sp)
    sd s11, 8(sp)

    sd sp, 0(a0)
    ld sp, 0(a1)

    ld s11, 8(sp)
    ld s10, 16(sp)
    ld s9, 24(sp)
    ld s8, 32(sp)
    ld s7, 40(sp)
    ld s6, 48(sp)
    ld s5, 56(sp)
    ld s4, 64(sp)
    ld s3, 72(sp)
    ld s2, 80(sp)
    ld s1, 88(sp)
    ld s0, 96(sp)
    ld ra, 104(sp)
    addi sp, sp, 112

    ret

.global thread_clone_and_switch
thread_clone_and_switch:
    /* a0 = parent, a1 = child, a2 = is_parent_offset */
    addi sp, sp, -112
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    sd s7, 40(sp)
    sd s8, 32(sp)
    sd s9, 24(sp)
    sd s10, 16(sp)
    sd s11, 8(sp)

    /* Call copy_kernel_stack(parent, child, parent_sp, offset) */
    /* ABI: a0=parent, a1=child, a2=parent_sp, a3=offset */
    mv a3, a2    /* offset -> a3 */
    mv a2, sp    /* current sp -> a2 (parent_sp) */
    
    call copy_kernel_stack
    
    /* a0 now contains the child's RSP (returned from copy_kernel_stack) */
    mv sp, a0

    /* We are now on the child's stack! */
    ld s11, 8(sp)
    ld s10, 16(sp)
    ld s9, 24(sp)
    ld s8, 32(sp)
    ld s7, 40(sp)
    ld s6, 48(sp)
    ld s5, 56(sp)
    ld s4, 64(sp)
    ld s3, 72(sp)
    ld s2, 80(sp)
    ld s1, 88(sp)
    ld s0, 96(sp)
    ld ra, 104(sp)
    addi sp, sp, 112

    ret
