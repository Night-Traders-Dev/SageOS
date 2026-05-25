# SageOS Build Pipeline

SageOS leverages the SageLang compiler to build its core components and architecture-specific kernels.

## Components
1.  **SageLang Compiler**: Located in `sageos_build/sage_lang/`. Compiles `.sage` scripts to native code or VM bytecode.
2.  **OS Build Module**: `os.boot.build` provides the logic for generating boot assembly, linker scripts, and kernel C stubs.

## Pipeline Flow
1.  **Script Selection**: A Sage script (e.g., `examples/boot/rpi4_demo.sage`) defines the target architecture and device.
2.  **File Generation**: The compiler generates `boot.S`, `kernel.c`, and `linker.ld`.
3.  **Compilation**: `aarch64-linux-gnu-gcc` (or relevant cross-compiler) compiles the C code and assembly.
4.  **Linking**: The linker produces a final ELF binary.
5.  **Artifact Management**: The `sageos.sh` script moves artifacts to the `build/` directory.
