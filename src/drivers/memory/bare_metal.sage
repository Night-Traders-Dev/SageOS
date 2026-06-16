# Bare Metal Primitives for SageOS
# Exposes patched MetalVM opcodes to SageLang.

import gpu

# Use repurposed GPU opcodes that the compiler already knows.
# We will match the opcode numbers in the Makefile patch.

proc peek64(addr):
    # Opcode 44 in patched MetalVM
    return gpu.key_pressed(addr)

proc poke64(addr, val):
    # Opcode 45 in patched MetalVM
    gpu.update_uniform(addr, val)

proc poke32(addr, val):
    # Opcode 46 in patched MetalVM
    gpu.cmd_push_constants(addr, val, 0, [])

proc halt():
    # Opcode 47 in patched MetalVM
    gpu.poll_events()

proc get_trap():
    # Opcode 48 in patched MetalVM
    return gpu.cmd_begin_render_pass()

proc enable_interrupts():
    # Opcode 49 in patched MetalVM (TODO: Patch Makefile for 49)
    gpu.cmd_end_render_pass()

proc set_timer(interval):
    # Opcode 50 in patched MetalVM (TODO: Patch Makefile for 50)
    gpu.cmd_draw(interval, 0, 0, 0)
