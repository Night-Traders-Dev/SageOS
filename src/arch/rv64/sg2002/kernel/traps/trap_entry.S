.section .text
.global trap_entry
.align 4

trap_entry:
    # Save registers to stack
    addi sp, sp, -256
    sd x1, 0(sp)
    sd x2, 8(sp)
    sd x3, 16(sp)
    # ... (more registers)

    # Call handle_trap
    csrr a0, scause
    csrr a1, sepc
    call handle_trap

    # Restore registers and return
    ld x1, 0(sp)
    # ... (more restores)
    addi sp, sp, 256
    sret
