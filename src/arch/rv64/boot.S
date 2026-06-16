.section .text.entry
.global _start
_start:
    # Heartbeat 'A'
    li t0, 0x10000000
    li t1, 0x41
    sb t1, 0(t0)

    # Set up stack
    la sp, _stack_top
    
    # Clear BSS
    la a0, _bss_start
    la a1, _bss_end
    bgeu a0, a1, 2f
1:
    sd zero, (a0)
    addi a0, a0, 8
    bltu a0, a1, 1b
2:
    # Jump to kmain
    call kmain

    # Loop forever if kmain returns
halt:
    wfi
    j halt

.section .bss
.align 16
_stack_bottom:
.skip 16384 # 16KB stack
_stack_top:
