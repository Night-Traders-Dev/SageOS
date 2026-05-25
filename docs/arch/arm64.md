# SageOS Architecture: ARM64

SageOS on ARM64 targets 64-bit ARMv8-A and newer processors. It focuses on modularity and leveraging architecture-specific features like Exception Levels (EL) for security and isolation.

## Target Hardware
- **Raspberry Pi 4 Model B**: Primary physical target.
- **QEMU `virt` machine**: Primary development and CI target.

## Execution Environment
- **EL3**: Typically handled by firmware (e.g., TF-A, RPi bootloader).
- **EL2**: Hypervisor level (if present).
- **EL1**: SageOS Kernel (Supervisor mode).
- **EL0**: User space applications.

## Memory Mapping
SageOS on ARM64 uses a 48-bit or 39-bit virtual address space.
- **Kernel Base**: `0xFFFF000000000000` (Typical high-memory kernel mapping).
- **Physical Load Address**:
  - RPi4: `0x80000`
  - Virt: `0x40000000`

## Drivers
- **UART**: PL011 PrimeCell UART for serial console output.
- **Interrupts**: GICv2 or GICv3 (Generic Interrupt Controller).
- **Timer**: ARM Generic Timer (arch-timer).

## Build Pipeline
The build pipeline utilizes `os.boot.build` from SageLang to generate:
1.  **Boot Stub**: Parks secondary cores and sets up the initial stack.
2.  **Runtime**: Minimal bare-metal Sage runtime.
3.  **Kernel**: The main OS logic.
