.global setjmp
setjmp:
    /* rdi = jmp_buf */
    movq %rbx, (0*8)(%rdi)
    movq %rbp, (1*8)(%rdi)
    movq %r12, (2*8)(%rdi)
    movq %r13, (3*8)(%rdi)
    movq %r14, (4*8)(%rdi)
    movq %r15, (5*8)(%rdi)
    leaq 8(%rsp), %rax     /* RSP as it was before the call */
    movq %rax, (6*8)(%rdi)
    movq (%rsp), %rax      /* Return address (RIP) */
    movq %rax, (7*8)(%rdi)
    xorq %rax, %rax        /* Return 0 */
    ret

.global longjmp
longjmp:
    /* rdi = jmp_buf, rsi = val */
    movq (0*8)(%rdi), %rbx
    movq (1*8)(%rdi), %rbp
    movq (2*8)(%rdi), %r12
    movq (3*8)(%rdi), %r13
    movq (4*8)(%rdi), %r14
    movq (5*8)(%rdi), %r15
    movq (6*8)(%rdi), %rsp
    movq (7*8)(%rdi), %rax /* Temporarily store RIP in RAX */
    
    /* Set return value */
    movq %rsi, %rdx
    testq %rdx, %rdx
    jnz 1f
    incq %rdx              /* val must be non-zero */
1:
    movq %rdx, %rax
    
    /* Jump to saved RIP */
    jmp *(7*8)(%rdi)
