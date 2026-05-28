/* toolchain/newlib/libc/sys/sageos/crt0.x64.S */
    .section .text
    .global _start
    .type   _start, @function

_start:
    /* Align stack to 16 bytes */
    andq    $-16, %rsp

    /* Clear BSS */
    leaq    __bss_start(%rip), %rdi
    leaq    _end(%rip), %rcx
    subq    %rdi, %rcx
    xorl    %eax, %eax
    rep stosd

    /* Load argc, argv (kernel puts them at initial rsp) */
    /* SageOS implementation: for now we assume argc=0, argv=NULL if not set up */
    movq    (%rsp), %rdi          /* argc */
    leaq    8(%rsp), %rsi         /* argv */
    leaq    8(%rsi,%rdi,8), %rdx  /* envp */

    call    main

    /* Pass return value to exit */
    movl    %eax, %edi
    call    _exit
    ud2                           /* should never reach here */

    .size _start, .-_start
