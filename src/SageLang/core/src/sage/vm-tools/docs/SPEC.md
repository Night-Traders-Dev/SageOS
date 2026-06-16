# SGVM & SGRV Specification

## 1. Overview
The Sage Virtual Machine (SGVM) is the portable execution substrate for SageOS. It supports two execution backends:
- **SVM**: Traditional Stack Virtual Machine.
- **SRVM**: RISC-V 64-bit Register Virtual Machine.

---

## 2. Binary Formats

### 2.1 SVM Format (.sgvm)
1. **Header**: "SGVM" (4 bytes).
2. **Version**: 2 bytes.
3. **Function Count**: 2 bytes (big-endian).
4. **Constant Pool**: Count + Entries.
5. **Chunk Count**: 4 bytes (big-endian).
6. **Chunks**: Length (4 bytes) + Code (Variable length).

### 2.2 SGRV Format (.sgrv)
1. **Header**: "SGRV" (4 bytes).
2. **Version**: 2 bytes.
3. **Constant Pool**: Count + Entries (same format as SVM).
4. **Chunk Count**: 4 bytes (big-endian).
5. **Chunks**: Length (4 bytes, big-endian) + Code (Array of 32-bit RISC-V instructions).

---

[Existing Content...]

## 2. Compilation & Execution Pipeline
SageLang code follows a strictly defined path to execution:
1. **Source**: Human-readable `.sage` files.
2. **Compiler Frontend**: Parses source into an Abstract Syntax Tree (AST) or Intermediate Representation (SGIR).
3. **Bytecode Generation**: Emits portable SGVM instructions.
4. **Verification**: Mandatory security and safety checks.
5. **Runtime Execution**: Execution by the MetalVM engine.

## 3. Bytecode Verification & Runtime Safety
(Note: Full static bytecode verification is currently a roadmap item for the SageLang-based interpreter. Runtime enforcement is currently used to ensure safety.)

Before and during execution, production SGVM bytecode MUST pass verification and runtime checks that ensure:
- **Control Flow Integrity**: No illegal jumps; recursive depth is limited to 1,024 frames (`max_call_depth`).
- **Type Safety**: Operations are performed on valid operand types.
- **Boundary Checks**: No out-of-bounds access to memory or object arenas. Operand stack depth is limited to 65,536 entries (`max_stack_depth`). Exception handler nesting is limited to 1,024 levels (`max_handler_depth`).
- **Capability Access**: The bytecode does not attempt to use restricted syscalls without proper permissions.

## 4. Execution Modes
SGVM supports multiple execution strategies:
- **Interpreted**: Direct execution of the AST or bytecode (default for kernel-mode scripts).
- **Threaded Interpreter**: High-performance bytecode dispatching using labels-as-values.
- **JIT/AOT (Future)**: Just-in-Time or Ahead-of-Time compilation to native machine code for performance-critical applications.

## 5. MetalVM C API (`metal_vm.h`)
The kernel interacts with SGVM via the following internal interfaces:
- `metal_vm_load_binary()`: Loads a compiled `.sgvm` artifact into memory.
- `metal_vm_run()` / `metal_vm_step()`: Executes bytecode instructions.
- `metal_vm_register_native()`: Binds kernel-level functions to SageLang (implemented via the **Native Bridge** in the SageLang-based interpreter).
- `sage_gil_acquire()` / `sage_gil_release()`: Serializes access to the VM state to maintain thread safety in SMP environments.

## 6. Object System & GC
SGVM features a reference-tracked object system with a built-in Mark-and-Sweep garbage collector. Objects (primarily Arrays and Dictionaries) are capability-tagged and reside in dedicated memory arenas.
