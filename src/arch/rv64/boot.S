.section .text.entry
.global _start
_start:
    # Save handoff structure pointer (passed in a0) in s0
    mv s0, a0

    # Heartbeat 'A'
    li t0, 0x10000000
    li t1, 0x41
    sb t1, 0(t0)

    # Set up stack
    la sp, _stack_top
    
    # Set trap vector
    la t0, trap_vector
    csrw stvec, t0
    
    # Delegate S-mode ECALLs (bit 9) to M-mode
    li t0, (1 << 9)
    csrw medeleg, t0
    
    # Clear BSS
    la a0, _bss_start
    la a1, _bss_end
    bgeu a0, a1, 2f
1:
    sd zero, (a0)
    addi a0, a0, 8
    bltu a0, a1, 1b
2:
    # Restore handoff structure pointer in a0 as argument to kmain
    mv a0, s0
    # Jump to kmain
    call kmain

    # Loop forever if kmain returns
halt:
    wfi
    j halt

.align 4
trap_vector:
    # Save context
    addi sp, sp, -32
    sd t0, 0(sp)
    sd t1, 8(sp)
    sd a0, 16(sp)
    sd a1, 24(sp)
    
    # UART Debug - Entry
    li t0, 0x10000000
    li t1, 'V'
    sb t1, 0(t0)

    # Increment sepc to skip the faulting instruction
    csrr t0, sepc
    addi t0, t0, 4
    csrw sepc, t0
    
    # Call handle_trap(cause, epc, sp)
    csrr a0, scause
    csrr a1, sepc
    mv a2, sp
    call handle_trap
    
    # UART Debug - Exit
    li t0, 0x10000000
    li t1, 'E'
    sb t1, 0(t0)
    
    # Restore context
    ld t0, 0(sp)
    ld t1, 8(sp)
    ld a0, 16(sp)
    ld a1, 24(sp)
    addi sp, sp, 32
    sret

.section .bss
.align 16
_stack_bottom:
.skip 16384 # 16KB stack
_stack_top:
