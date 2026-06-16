# SageOS Architecture

## Overview
SageOS follows a microkernel-inspired design where the core "kernel" is a small, verifiable SRVM bytecode module.

## Layering
1. **Hardware (RV64, x64, ARM64)**
2. **MetalVM (C Shim)**: Provides the SRVM execution environment and raw hardware access.
3. **SageOS Core (Pure Sage)**: Manages tasks, capabilities, and IPC.
4. **System Services (Pure Sage)**: VFS, Networking, Driver Manager.

## RISC-V 64 (RV64) Implementation
The RV64 port targets the `virt` machine in QEMU.
- **Base Address**: `0x80200000` (Loaded by OpenSBI).
- **Boot Protocol**: 
  1. OpenSBI initializes the hardware and jumps to the entry point in Supervisor mode.
  2. `arch/rv64/boot.S` sets up the stack and clears the BSS.
  3. `arch/rv64/metal_shim.c` (kmain) initializes the MetalVM and loads the kernel bytecode.
- **Paging**: Sv39/Sv48 (Not yet enabled).
- **Console**: SBI-based putchar for Supervisor mode heartbeats; MMIO 16550A UART for low-level debug.
- **Binary Format**: The SageOS kernel is compiled to SGVM bytecode and embedded into the ELF `.data` section using `objcopy`.
