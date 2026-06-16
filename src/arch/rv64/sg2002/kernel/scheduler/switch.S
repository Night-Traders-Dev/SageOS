.section .text
.global switch_to
.align 4

switch_to:
    # a0: old_task_sp, a1: new_task_sp
    # Save current registers
    addi sp, sp, -128
    sd ra, 0(sp)
    # ... (save all callee-saved registers)
    sd sp, (a0)

    # Restore new task
    ld sp, (a1)
    # ... (restore all callee-saved registers)
    ld ra, 0(sp)
    addi sp, sp, 128
    ret
