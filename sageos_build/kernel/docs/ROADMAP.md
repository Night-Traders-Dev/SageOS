# SGVM Roadmap: Unsupported Features

This document outlines the features and standard library modules currently unsupported by the SGVM interpreter (`sgvm.sage`). Tasks are categorized by their implementation difficulty.

## ✅ Supported Features
- **Floating Point Binary Loading**: Full IEEE 754 64-bit double support for packing and unpacking.
- **Exception Handling**: Full support for `try/catch/finally` via `SETUP_TRY`, `END_TRY`, and `RAISE`.
- **Enhanced Flow Control**: Correct handling of relative jumps and loop control opcodes.
- **Dynamic Imports**: Ability to load and execute external `.sgvm` modules at runtime.
- **Bytecode Verification**: Pre-execution safety checks for constant references and jump targets.
- **Native Bridging Layer**: Full support for `math`, `io`, `sys`, and `re` modules via host SageLang mapping.
- **Optimized Strings**: Native bridging for string `find`, `replace`, and `split` operations.
- **Multi-threading**: Full support for `thread` module with a Global Interpreter Lock (GIL) and result capturing.
- **FFI & Memory Interop**: Safe bridging to host C libraries via `ffi`, `mem`, and `struct` modules.
- **Resource Management**: Guest memory tracking and limits (`mem.limit`), plus GC control primitives.

## 🟡 Medium Difficulty (Native Bridging)
(All tasks currently completed)

## 🔴 High Difficulty (Complex Systems)
These tasks involve significant architectural additions or complex resource management.

- **GPU Hot-Paths**: Implementation of the `BC_OP_GPU_*` opcodes for Vulkan/OpenGL acceleration. This requires a handle-based resource manager within the VM.
- **Networking**: Full implementation of `net`, `socket`, and `http` modules, requiring a safe abstraction of the host's networking stack.

## 🟣 Research / Future
- **JIT/AOT Compilation**: Transitioning from a pure interpreter to a system that emits native machine code for the target architecture.
- **Formal Verification**: Tools to mathematically prove the safety of a `.sgvm` binary before execution (as mentioned in `SPEC.md`).
