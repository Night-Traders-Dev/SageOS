# SageOS Release 0.4.4

## Overview
SageOS 0.4.4 is a major architectural refinement release, focusing on memory safety, system robustness, and developer experience. It unifies memory allocation, enables interactive SageLang development within the kernel, and automates partition detection.

## Key Changes
- **Unified Memory Allocation**: Introduced `sage_alloc.c`, a centralized arena allocator that replaces fragmented and fragile allocation logic in the kernel and SageLang shims. This improves stability and provides predictable memory behavior for the Sage interpreter.
- **Integrated Sage REPL**: The full SageLang AST interpreter is now part of the kernel build across all architectures. Users can now enter an interactive Sage shell directly from the OS prompt.
- **Dynamic Partition Detection**: Implemented MBR parsing in the FAT32 driver. The kernel no longer relies on hardcoded LBA 2048 and can dynamically find FAT32/ESP partitions on any standard disk.
- **Synchronized SGVM Pipeline**: The `compile_to_sgvm.py` tool now dynamically parses opcode definitions from the SageLang compiler source, preventing bytecode corruption due to opcode desync.
- **Freestanding Hygiene**: Refactored `metal_vm.c` and kernel stubs to strictly adhere to freestanding invariants, removing dependencies on host-system headers like `<ctype.h>`.

## Architecture Status
- **x64**: Fully functional virt target. Dynamic partition detection verified.
- **arm64**: Fully functional virt target. Integrated atomic stubs for SageLang.
- **rv64**: Fully functional virt target.

## Developer Notes
- Use `sage` in the shell to enter the new Interactive REPL.
- Source `.sage` files can now be executed directly via `sage run <path>`, with the system automatically using the AST interpreter if a pre-compiled SGVM artifact is missing.
