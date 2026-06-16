# SageOS Specification (SPEC)

## 1. Execution Model
SageOS runs primarily on the **SageVM (SRVM)**. The kernel and all system services are compiled from SageLang into SRVM bytecode.

## 2. Boot Protocol (RV64)
1. **Firmware Stage**: QEMU loads OpenSBI at `0x80000000`.
2. **Kernel Loading**: OpenSBI loads the SageOS ELF and jumps to `0x80200000` in Supervisor mode.
3. **Boot Stub**: `arch/rv64/boot.S` initializes the stack (`sp`) and clears the BSS section.
4. **MetalVM Initialization**: `kmain` in `arch/rv64/metal_shim.c` initializes the fixed-size pools of the MetalVM.
5. **Bytecode Execution**: The embedded `.sgvm` bytecode is loaded and verified, then executed by the VM loop.

## 3. Capability-Based Security
All resource access (Memory, I/O, IPC) is governed by capabilities. A task must possess a valid capability to perform any privileged operation.

## 4. Hardware Abstraction Layer (HAL)
The HAL consists of "MetalVM" native interfaces. SageLang code invokes these interfaces for:
- Physical memory mapping.
- Context switching.
- Hardware I/O (MMIO).
- Interrupt management.
