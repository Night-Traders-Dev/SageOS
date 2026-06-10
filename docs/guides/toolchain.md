# SageOS Native Toolchain

SageOS now includes a native GCC-based toolchain that allows for self-hosting and on-device development. This document covers how the toolchain is built, distributed, and integrated into the SageOS environment.

## 1. Overview

The toolchain consists of:
- **Binutils 2.42**: Assembler (`as`), Linker (`ld`), and object utilities.
- **GCC 14.1.0**: C compiler with SageOS-specific patches.
- **Newlib 4.4.0**: A lightweight C library ported to SageOS.

Supported Architectures:
- `x86_64-unknown-sageos`
- `aarch64-unknown-sageos`
- `riscv64-unknown-sageos`

## 2. Integration into SageOS

The toolchain is installed into the SageOS disk image (`virt.img`) under the `/usr` hierarchy:
- `/usr/bin/`: Compiler and binutils executables.
- `/usr/lib/`: Standard libraries and GCC support files.
- `/usr/include/`: C library headers.
- `/usr/libexec/`: Internal compiler backends (e.g., `cc1`).

### SageShell Support
The SageShell has been enhanced to execute ELF binaries directly from the filesystem. If you type a command like `gcc` or `as`, the shell will search in `/bin` and `/usr/bin` to find and execute the corresponding toolchain binary.

## 3. Building from Source

If you wish to build the toolchain yourself instead of using prebuilt binaries:

### Cross-Toolchain (Host to SageOS)
To build a toolchain that runs on your Linux host and targets SageOS:
```bash
./toolchain/build_toolchain.sh [x86_64|aarch64|riscv64] /path/to/install
```

### Native Toolchain (Runs inside SageOS)
To build a toolchain that runs natively inside SageOS:
```bash
./toolchain/build_native.sh [x86_64|aarch64|riscv64]
```
The resulting files will be placed in `/home/kraken/sageos-native-dist`.

## 4. Packaging and Distribution

Toolchains are packaged into tarballs and hosted as GitHub Release assets.

### Automatic Retrieval
The SageOS build script (`sageos.sh`) and toolchain installer (`scripts/install_toolchain.sh`) are designed to automatically download the appropriate prebuilt tarball from GitHub if a local toolchain is not found.

### Manual Packaging
To regenerate the tarballs for all architectures:
```bash
./scripts/package_toolchains.sh
```

## 6. SageLang Integration

SageOS is tightly integrated with the **SageLang** ecosystem (v3.6.1+). The build system automatically bootstraps the `sage` compiler and uses it to compile system components into SGVM bytecode.

Key integration points:
- **`sgvm`**: The native Sage Virtual Machine, used for executing system scripts and applications.
- **`sgvmc`**: The SageVM compiler, which transforms `.sage` sources into optimized `.sgvm` binaries.
- **Native Bridges**: SageOS provides a high-performance C bridge for `math`, `io`, `thread`, and `sys` modules, enabling Sage scripts to interact directly with kernel services.
