.global syscall_entry
syscall_entry:
    /* On x64 with `syscall` instruction:
     * rax = syscall number
     * rdi, rsi, rdx, r10, r8, r9 = args
     * Return value in rax
     */
    push %rbp
    mov %rsp, %rbp
    
    /* Save caller-save registers (rcx and r11 are destroyed by syscall) */
    push %rcx
    push %r11
    
    /* Call C dispatcher
     * Linux ABI uses r10 for the 4th argument instead of rcx in syscalls.
     * Our C dispatcher: syscall_dispatch(num, a1, a2, a3, a4, a5)
     * num: rax -> %rdi
     * a1:  rdi -> %rsi
     * a2:  rsi -> %rdx
     * a3:  rdx -> %rcx
     * a4:  r10 -> %r8
     * a5:  r8  -> %r9
     */
    mov %r8, %r9
    mov %r10, %r8
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi
    mov %rax, %rdi
    
    call syscall_dispatch
    
    pop %r11
    pop %rcx
    pop %rbp
    sysretq
