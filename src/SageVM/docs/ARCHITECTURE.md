# SageVM Architecture

SageVM implements a dual-VM architecture, supporting both a traditional stack-based virtual machine (SVM) and a register-based virtual machine (SRVM).

## 1. SVM (Stack Virtual Machine - Legacy/Core)
The traditional SGVM architecture optimized for code density. Uses variable-length bytecode and a stack-based operand model. This is the primary target for initial compilation from SageLang source.

## 2. SRVM (RISC-V Register Virtual Machine - Modern/High-Performance)
A modern, register-based architecture mapped to the RV64I specification. This VM is the target for the AOT/JIT compilation pipeline, providing superior performance and easier mapping to native hardware instructions (RISC-V, x86_64, ARM64).

- **Register File**: 32 x 64-bit general-purpose registers (x0-x31).
- **Instruction Encoding**: Fixed 32-bit width for efficient decoding.
- **Addressing**: Load-Store architecture separating computation from memory.

## 3. Unified Diagnostic Pipeline
To support the ongoing development of both architectures, SageVM provides a unified diagnostic pipeline. The `sagevm` CLI automatically detects the target architecture by inspecting the 4-byte magic header of the binary.

- **SGVM (Stack)**: Magic `SGVM`. Disassembled into high-level `.sage`-like pseudo-code or low-level `.svm` bytecode.
- **SGRV (RISC-V)**: Magic `SGRV`. Disassembled into standard RISC-V assembly with custom SageVM system extensions (`ldc`, `vm_nop`, etc.).

This pipeline ensures that developers can inspect the low-level structure of their compiled programs regardless of the target architecture.

---
[Existing Content...]

(Refer to `SPEC.md` for the complete opcode table.)

### Custom VMSYS Opcodes
SRVM uses `OP_VMSYS` (standard RISC-V SYSTEM opcode repurposed) to access SageVM-specific system functionality:
- `funct3 = 000` (VM Operations): HALT, PRINT, PRINTM, CALL, PUSH_ENV, POP_ENV, SETUP_TRY, END_TRY, RAISE.
- `funct3 = 001` (GPU Operations): Standard 28 GPU opcodes.
- `funct3 = 010` (Object Operations): GET_GLOBAL, SET_GLOBAL, GET_PROP, SET_PROP, ARRAY_NEW, etc.

---
[Existing Content...]

## Opcodes

The following opcodes are supported by `sgvm.sage` and emitted by `sgvmc.sage`:

| Opcode | Value | Description |
|--------|-------|-------------|
| OP_CONSTANT | 0 | Push a constant onto the stack |
| OP_NIL | 1 | Push nil onto the stack |
| OP_TRUE | 2 | Push true onto the stack |
| OP_FALSE | 3 | Push false onto the stack |
| OP_POP | 4 | Pop the top value from the stack |
| OP_GET_GLOBAL | 5 | Get a global variable value |
| OP_DEFINE_GLOBAL | 6 | Define a global variable |
| OP_SET_GLOBAL | 7 | Assign a value to a global variable |
| OP_DEFINE_FUNCTION | 8 | Define a function |
| OP_GET_PROPERTY | 9 | Get an object property |
| OP_SET_PROPERTY | 10 | Set an object property |
| OP_GET_INDEX | 11 | Get an element at an index (array/dict) |
| OP_SET_INDEX | 12 | Set an element at an index |
| OP_LOAD_FUNCTION | 13 | Load a function onto the stack |
| OP_SLICE | 14 | Perform an array or string slice |
| OP_ADD | 15 | Addition |
| OP_SUB | 16 | Subtraction |
| OP_MUL | 17 | Multiplication |
| OP_DIV | 18 | Division |
| OP_MOD | 19 | Modulo |
| OP_NEGATE | 20 | Unary negation |
| OP_EQUAL | 21 | Equality check |
| OP_NOT_EQUAL | 22 | Inequality check |
| OP_GREATER | 23 | Greater than |
| OP_GREATER_EQUAL | 24 | Greater than or equal |
| OP_LESS | 25 | Less than |
| OP_LESS_EQUAL | 26 | Less than or equal |
| OP_BIT_AND | 27 | Bitwise AND |
| OP_BIT_OR | 28 | Bitwise OR |
| OP_BIT_XOR | 29 | Bitwise XOR |
| OP_BIT_NOT | 30 | Bitwise NOT |
| OP_SHIFT_LEFT | 31 | Bitwise left shift |
| OP_SHIFT_RIGHT | 32 | Bitwise right shift |
| OP_NOT | 33 | Logical NOT |
| OP_TRUTHY | 34 | Truthiness check |
| OP_JUMP | 35 | Unconditional jump |
| OP_JUMP_IF_FALSE | 36 | Jump if the top value is false |
| OP_CALL | 37 | Call a function |
| OP_CALL_METHOD | 38 | Call an object method |
| OP_ARRAY | 39 | Create an array |
| OP_TUPLE | 40 | Create a tuple |
| OP_DICT | 41 | Create a dictionary |
| OP_PRINT | 42 | Print a value |
| OP_EXEC_AST_STMT | 43 | Execute an AST statement (fallback) [NEW] |
| OP_RETURN | 44 | Return from a function |
| OP_PUSH_ENV | 45 | Push a new environment scope |
| OP_POP_ENV | 46 | Pop an environment scope |
| OP_DUP | 47 | Duplicate a value on the stack |
| OP_ARRAY_LEN | 48 | Get array length |
| OP_BREAK | 49 | Loop break |
| OP_CONTINUE | 50 | Loop continue |
| OP_LOOP_BACK | 51 | Jump to the start of a loop |
| OP_IMPORT | 52 | Import and execute an external module |
| OP_CLASS | 53 | Define a class |
| OP_METHOD | 54 | Define a method on a class |
| OP_INHERIT | 55 | Set up class inheritance |
| OP_SETUP_TRY | 56 | Push an exception handler |
| OP_END_TRY | 57 | Pop the current exception handler |
| OP_RAISE | 58 | Raise an exception |
| OP_GPU_POLL_EVENTS | 59 | gpu.poll_events() |
| OP_GPU_WINDOW_SHOULD_CLOSE | 60 | gpu.window_should_close() -> bool |
| OP_GPU_GET_TIME | 61 | gpu.get_time() -> number |
| OP_GPU_KEY_PRESSED | 62 | gpu.key_pressed(key) -> bool |
| OP_GPU_KEY_DOWN | 63 | gpu.key_down(key) -> bool |
| OP_GPU_MOUSE_POS | 64 | gpu.mouse_pos() -> dict{x,y} |
| OP_GPU_MOUSE_DELTA | 65 | gpu.mouse_delta() -> dict{x,y} |
| OP_GPU_UPDATE_INPUT | 66 | gpu.update_input() |
| OP_GPU_BEGIN_COMMANDS | 67 | gpu.begin_commands(cmd) |
| OP_GPU_END_COMMANDS | 68 | gpu.end_commands(cmd) |
| OP_GPU_CMD_BEGIN_RP | 69 | gpu.cmd_begin_render_pass(cmd, rp, fb, w, h, clear) |
| OP_GPU_CMD_END_RP | 70 | gpu.cmd_end_render_pass(cmd) |
| OP_GPU_CMD_DRAW | 71 | gpu.cmd_draw(cmd, verts, inst, first_v, first_i) |
| OP_GPU_CMD_BIND_GP | 72 | gpu.cmd_bind_graphics_pipeline(cmd, pipe) |
| OP_GPU_CMD_BIND_DS | 73 | gpu.cmd_bind_descriptor_set(cmd, layout, set, bp) |
| OP_GPU_CMD_SET_VP | 74 | gpu.cmd_set_viewport(cmd, x, y, w, h, mind, maxd) |
| OP_GPU_CMD_SET_SC | 75 | gpu.cmd_set_scissor(cmd, x, y, w, h) |
| OP_GPU_CMD_BIND_VB | 76 | gpu.cmd_bind_vertex_buffer(cmd, buf) |
| OP_GPU_CMD_BIND_IB | 77 | gpu.cmd_bind_index_buffer(cmd, buf) |
| OP_GPU_CMD_DRAW_IDX | 78 | gpu.cmd_draw_indexed(cmd, idx_count, ...) |
| OP_GPU_SUBMIT_SYNC | 79 | gpu.submit_with_sync(cmd, wait, signal, fence) |
| OP_GPU_ACQUIRE_IMG | 80 | gpu.acquire_next_image(sem) -> number |
| OP_GPU_PRESENT | 81 | gpu.present(sem, img_idx) |
| OP_GPU_WAIT_FENCE | 82 | gpu.wait_fence(fence, timeout) |
| OP_GPU_RESET_FENCE | 83 | gpu.reset_fence(fence) |
| OP_GPU_UPDATE_UNIFORM | 84 | gpu.update_uniform(handle, data) |
| OP_GPU_CMD_PUSH_CONST | 85 | gpu.cmd_push_constants(cmd, layout, stages, data) |
| OP_GPU_CMD_DISPATCH | 86 | gpu.cmd_dispatch(cmd, gx, gy, gz) |
| OP_MATH_PRINTM | 87 | math.printm(matrix) [SageVM Extension] |
| OP_HALT | 0xFF | Halt execution [SageVM Extension] |

## Native Bridge

SGVM provides a high-performance native bridge to the host SageLang environment. This allows guest bytecode to call standard library functions directly without the overhead of guest-side implementations.

The following modules are currently bridged:
- **math**: Native SageLang `math` module.
- **io**: Native SageLang `io` module.
- **sys**: Native SageLang `sys` module.
- **net**: Native SageLang `net` module.
- **thread**: Native SageLang `thread` module (host-level threading).
- **gpu**: Native SageLang `gpu` module (Vulkan/OpenGL acceleration).
- **ml_native**: Native SageLang `ml_native` module (Machine Learning acceleration).
- **gc**: (Experimental stub) Native SageLang garbage collector interface.
- **reflect**: (Experimental stub) Native SageLang reflection interface.

*Note: Modules like `re`, `ffi`, `mem`, and `struct` are not currently exposed through the guest-to-host bridge in this implementation.*

Native bridging is implemented by tagging objects and function-like dictionaries with a `__native__` property. The `OP_CALL` and `OP_CALL_METHOD` opcodes detect these tags and dispatch execution to the VM's `call_native()` handler.

## Core Builtins

SGVM exposes several host-level functions directly in the global scope for performance and parity with the SageLang environment:

| Builtin | Description |
|---------|-------------|
| `clock()` | Returns the current system clock time. |
| `str(val)` | Converts a value to its string representation. |
| `int(val)` | Converts a value to an integer. |
| `tonumber(val)` | Converts a string to a number. |
| `len(obj)` | Returns the length of an array, dictionary, or string. |
| `print(val)` | Prints a value to standard output. |
| `range(n)` | Generates a range object from 0 to n-1. |
| `type(val)` | Returns the type name of a value. |

## Multi-threading & GIL

SGVM supports true multi-threading by leveraging the host SageLang `thread` module. Each guest thread runs in its own `MetalVM` instance, sharing the same constant pool and global environment.

To maintain consistency within the SageLang-based interpreter, a **Global Interpreter Lock (GIL)** is used to serialize guest bytecode execution. The GIL is automatically released during blocking operations such as `thread.sleep()`, `thread.join()`, and `thread.lock()` to allow for maximum concurrency.

## Sandboxing & Isolation

For high-isolation environments, SGVM provides several security features:
- **Resource Limits**: The host environment can enforce memory allocation limits on the guest VM.
- **Module Restriction**: Access to sensitive host modules can be restricted by omitting them from the global scope during VM initialization.
- **Internal Execution Limits**: To prevent Denial of Service (DoS) via resource exhaustion, the VM enforces the following internal limits:
  - **Maximum Stack Depth**: 65,536 (Maximum depth of the operand stack).
  - **Maximum Call Depth**: 1,024 (Maximum recursion depth for function calls).
  - **Maximum Handler Depth**: 1,024 (Maximum nesting depth for exception handlers).

## Function Arguments

Arguments to SGVM functions are passed positionally and bound to the function's local scope using the naming convention `__argN`, where `N` is the zero-based index of the argument. For example, the first argument is accessible as `__arg0`, the second as `__arg1`, and so on.

## Binary Format & I/O

SGVM uses a dedicated **BYTES** type for high-performance binary I/O, ensuring that bytecode and constants are parsed correctly across backends. The native `io.readbytes` bridge returns this type instead of a standard string, allowing for integer-based byte indexing.

The SGVM binary format consists of:
0. Optional Shebang: `#!/usr/bin/env sgvm\n` (added by `sgvmc --shebang`)
1. Header: "SGVM" (4 bytes)
2. Version: 0x01 0x00 (2 bytes)
3. Function Count: (2 bytes, big-endian) — Total number of function chunks.
4. Constant Pool:
   - Count (2 bytes, big-endian)
   - Constants (Type, then value)
5. Chunk Count: (4 bytes, big-endian) — Total number of chunks (including functions).
6. Chunks:
   - Length (4 bytes, big-endian)
   - Code (Byte array)
