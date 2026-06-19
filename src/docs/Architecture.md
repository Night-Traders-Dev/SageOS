# SageOS Architecture

## Overview
SageOS follows a microkernel-inspired design where the core "kernel" is a small, verifiable SRVM bytecode module.

## Layering
1. **Hardware (RV64, x64, ARM64)**
2. **MetalVM (C Shim)**: Provides the SRVM execution environment and raw hardware access.
3. **SageOS Core (Pure Sage)**: Manages tasks, capabilities, and IPC.
4. **System Services (Pure Sage)**: VFS, Networking, Driver Manager.

## Boot Architecture (SageBoot)
SageOS uses **SageBoot** as its unified, secure, modular bootloader across architectures.

### Boot Sequence
1. **Platform Firmware / Hypervisor** loads `SageBoot` as the primary boot payload:
   - **RISC-V 64**: OpenSBI jumps to `0x80200000` (S-Mode).
   - **ARM64 (AArch64)**: QEMU jumps to `0x40000000` (EL1/Device-mode).
2. **SageBoot Init**:
   - Disables interrupts, configures UART console, enables FPU to support floating-point operations.
   - Performs basic RAM stability diagnostics.
   - Presents an interactive boot menu with optional configurations.
3. **Kernel Loading & Verification**:
   - Parses the target `sageos_*.elf` image from memory (mapped via QEMU device loader).
   - Validates cryptographic signatures/checksums.
   - Relocates ELF segments to destination RAM:
     - **RISC-V 64**: Kernel relocates to `0x80800000`.
     - **ARM64**: Kernel relocates to `0x40200000`.
   - Zero-fills BSS segment padding.
4. **Handoff Structure (`SAGEOSBI`)**:
   - SageBoot constructs a standardized handoff block in memory:
     - `magic` (8 bytes): `0x534147454F534249` ("SAGEOSBI")
     - `kernel_start` (8 bytes)
     - `kernel_entry` (8 bytes)
     - `kernel_size` (8 bytes)
     - `cmdline_ptr` (8 bytes, points to boot parameter string)
     - `mmap_start` (8 bytes)
     - `mmap_size` (8 bytes)
5. **Control Transfer**:
   - Passes the physical address of the `SAGEOSBI` handoff block via register:
     - **RISC-V 64**: Register `a0`
     - **ARM64**: Register `x0`
     - **x86_64**: Register `rdi`
   - Jumps to the kernel entry point.

## RISC-V 64 (RV64) Implementation
The RV64 port targets the `virt` machine in QEMU.
- **Base Address**: `0x80800000` (Loaded via SageBoot handoff).
- **Paging**: Sv39/Sv48 (Not yet enabled).
- **Console**: SBI-based putchar for Supervisor mode heartbeats; MMIO 16550A UART for low-level debug.
- **Binary Format**: The SageOS kernel is compiled to SGVM bytecode and embedded into the ELF `.data` section using `objcopy`.

