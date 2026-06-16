.align 4
.global thread_switch
thread_switch:
    /* rdi = &prev->rsp, rsi = &next->rsp */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq (%rsi), %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret

.global thread_clone_and_switch
thread_clone_and_switch:
    /* rdi = parent, rsi = child, rdx = is_parent_offset */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Call copy_kernel_stack(parent, child, parent_sp, offset) */
    /* ABI: rdi=parent, rsi=child, rdx=parent_sp, rcx=offset */
    movq %rdx, %rcx    /* offset -> rcx */
    movq %rsp, %rdx    /* current rsp -> rdx (parent_sp) */
    
    call copy_kernel_stack
    /* rax = child_sp */

    /* Switch to child stack.
       The parent is "suspended" with its rsp saved in parent->rsp by copy_kernel_stack.
       When the parent resumes via thread_switch, it will return to sys_vfork. */
    movq %rax, %rsp

    /* In child now. Return to sys_vfork. */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
