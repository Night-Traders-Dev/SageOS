# SageOS - The Multi-Architecture Operating System

SageOS is a lightweight, modular operating system project. This repository is the central hub containing the **architecture-agnostic core** and linking to architecture-specific ports via submodules.

## Project Structure

- **`sageos_build/kernel/`**: The shared core components (SageLang, VFS, Shell, etc.).
- **`arch/`**: Architecture-specific ports (Git Submodules).
  - **[x64](https://github.com/Night-Traders-Dev/SageOS_x64)**: Intel/AMD 64-bit.
  - **[arm64](https://github.com/Night-Traders-Dev/SageOS_arm64)**: ARM 64-bit (RPi4).
  - **[rv64](https://github.com/Night-Traders-Dev/SageOS_rv64)**: RISC-V 64-bit.

## Core Components (Agnostic)

These components are shared across all architectures:
- **SageLang VM**: High-performance bytecode execution engine.
- **VFS Layer**: Virtual Filesystem with FAT32 and BTRFS support.
- **SageShell**: Kernel-resident shell and diagnostic environment.
- **System Libraries**: Standard SageLang scripts and utilities.

## Getting Started

To clone SageOS with all architecture ports:
```bash
git clone --recursive https://github.com/Night-Traders-Dev/SageOS.git
```

### Developing for a Specific Architecture
The architecture ports are located in the `arch/` directory. Each submodule points to its respective repository's `main` branch, which is designed to build against the agnostic core in the parent directory.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
