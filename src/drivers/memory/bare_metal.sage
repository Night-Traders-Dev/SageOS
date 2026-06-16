# Bare Metal Primitives for SageOS
# Exposes patched MetalVM opcodes to SageLang.

import gpu

# Use repurposed GPU opcodes that the compiler already knows.
# We will match the opcode numbers in the Makefile patch.

proc peek64(addr):
    # Opcode 44 in patched MetalVM
    # Use gpu.key_pressed which emits BC_OP_GPU_KEY_PRESSED
    return gpu.key_pressed(addr)

proc poke64(addr, val):
    # Opcode 45 in patched MetalVM
    # Use gpu.update_uniform which emits BC_OP_GPU_UPDATE_UNIFORM
    gpu.update_uniform(addr, val)

proc poke32(addr, val):
    # Opcode 46 in patched MetalVM
    # Use gpu.cmd_push_constants which emits BC_OP_GPU_CMD_PUSH_CONST
    gpu.cmd_push_constants(addr, val, 0, [])

proc halt():
    # Opcode 47 in patched MetalVM
    # Use gpu.poll_events which emits BC_OP_GPU_POLL_EVENTS
    gpu.poll_events()

proc get_trap():
    # Opcode 48 in patched MetalVM
    # Use gpu.cmd_begin_render_pass which emits BC_OP_GPU_CMD_BEGIN_RP
    return gpu.cmd_begin_render_pass()
