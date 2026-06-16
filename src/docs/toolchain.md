# SageOS Toolchain

## SageLang Compiler
The project uses `sagelang` (via the `sgvmc` wrapper) to compile `.sage` files into binary SRVM bytecode (`.sgvm`).

## SageVM Runtime (MetalVM)
The runtime environment is built from the `MetalVM` sources (C). A patched version is generated during build to support specific opcodes like `OP_PRINT`.

## Build System
A `Makefile` is provided to orchestrate the build process.

```bash
# Build the RV64 kernel
make ARCH=rv64

# Clean build artifacts
make clean

# Run in QEMU
make run
```

## Test Suite
A Python-based test runner is used to execute Pure Sage tests on the target architecture.

```bash
# Run all tests
python3 tests/run_tests.py
```
