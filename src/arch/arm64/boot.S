.section .text
.global _start
_start:
    // Save handoff structure pointer (passed in x0) in x19
    mov x19, x0

    # Enable FP/SIMD (CPACR_EL1)
    mov x0, #3 << 20
    msr cpacr_el1, x0
    isb

    # UART0 Base Address for QEMU 'virt' machine
    ldr x0, =0x09000000
    mov w1, #'A'
    strb w1, [x0]

    # Set up stack
    ldr x0, =_stack_top
    mov sp, x0

    # Clear BSS
    ldr x0, =_bss_start
    ldr x1, =_bss_end
1:
    cmp x0, x1
    b.hs 2f
    str xzr, [x0], #8
    b 1b
2:
    // Restore handoff structure pointer in x0 as argument to kmain
    mov x0, x19
    # Jump to kmain
    bl kmain

halt:
    wfe
    b halt

.section .bss
.align 16
_stack_bottom:
.skip 16384
_stack_top:
