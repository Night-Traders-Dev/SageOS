# SageOS

SageOS is a modern, capability-based operating system built from the ground up using **SageLang** and targeting the **SageVM** (SRVM) execution environment.

## Vision
To create a high-performance, secure, and portable OS where the majority of the system logic is expressed in Pure Sage, minimizing the reliance on C for everything except the most fundamental hardware shims.

## Targets
- **RV64** (Primary focus)
- **x86_64**
- **ARM64**

## Project Structure
- `arch/`: Architecture-specific boot and hardware shims.
- `kernel/`: Pure Sage kernel implementation.
- `docs/`: System specifications and architectural design.
- `tests/`: Comprehensive test suite for all subsystems.
- `toolchain/`: Preserved build tools and compilers.

## Booting via SageBoot
SageOS integrates **SageBoot** as a submodule bootloader. When running, SageBoot is dynamically compiled for the target architecture (`ARCH`) and loaded as the primary `-kernel` payload in QEMU, while the SageOS kernel ELF is loaded into memory using QEMU's `-device loader`:

```bash
# Build and run SageOS with SageBoot for RISC-V 64
make clean && make run ARCH=rv64

# Build and run SageOS with SageBoot for ARM64
make clean && make run ARCH=arm64
```

