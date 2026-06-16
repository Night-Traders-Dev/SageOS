.section .text
.code64
.global _start
_start:
    cli
    # Simple Heartbeat 'A' via COM1 (0x3F8)
    mov $0x3F8, %dx
    mov $'A', %al
    out %al, (%dx)

    # Set up stack (64-bit)
    movabsq $_stack_top, %rax
    mov %rax, %rsp

    # Clear BSS (64-bit)
    movabsq $_bss_start, %rdi
    movabsq $_bss_end, %rcx
    sub %rdi, %rcx
    xor %rax, %rax
    rep stosb

    # Jump to kmain
    call kmain

halt:
    hlt
    jmp halt

.section .bss
.align 16
_stack_bottom:
.skip 16384
_stack_top:
