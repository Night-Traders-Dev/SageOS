# SageVM: Unified Virtual Machine Substrate

SageVM is a high-performance, pure SageLang implementation of the Sage Virtual Machine (SGVM). It provides a portable execution substrate for SageOS, supporting both traditional stack-based and modern RISC-V register-based architectures.

## Features (v0.9.8)

- **Dual-Architecture Engine**: Seamlessly switch between Stack VM (SVM) and RISC-V Register VM (SRVM).
- **100% Opcode Parity**: Supports all 89 SVM opcodes and the standard RV64I base instruction set.
- **Delegation Bridge**: Full guest-to-host delegation for GPU, I/O, and native modules.
- **OOP & Exceptions**: Native support for classes, inheritance, and exception handling (`try/catch/finally`) across both architectures.
- **Matrix Visualization**: Native `math.printm` support for formatted matrix output.
- **Unified CLI**: A single tool to compile, run, and debug both `.sgvm` (stack) and `.sgrv` (RISC-V) binaries.

## Installation

SageVM requires SageLang **v3.7.7** or higher. To build and install:

```bash
./sagemake --install
```

This produces the `sagevm` binary (and symlinks for `sgvm`/`sgvmc`).

## Unified CLI (sagevm)

- **`sagevm run <file.sgvm|.sgrv>`**: Execute a compiled binary. Auto-detects architecture via magic headers.
- **`sagevm compile <file.sage> [--riscv]`**: Compile source to binary. Use `--riscv` for register-based output.
- **`sagevm dis <file.sgvm|.sgrv>`**: Disassemble binary into readable instructions.
- **`sagevm hex <file.sgvm|.sgrv>`**: Low-level binary hexdump. Auto-detects magic.
- **`sagevm version`**: Show version information.

## Architectures

### 1. SVM (Stack Virtual Machine)
The traditional SGVM architecture. Uses variable-length bytecode and a 65,536-entry operand stack. Optimized for code density and simple compiler targets.

### 2. SRVM (Sage RISC-V Virtual Machine)
A modern, register-based architecture based on the **RV64I** specification.
- **32 x 64-bit registers**: Standard RISC-V register file mapping.
- **Fixed-width encoding**: 32-bit instructions for simplified decoding and high-speed dispatch.
- **Performance**: Up to 30-40% faster interpretation for arithmetic-heavy code.

## Performance Benchmarks (v0.9.8)

| Benchmark | SVM (ms) | SRVM (ms) | Improvement |
|-----------|----------|-----------|-------------|
| `fibonacci(22)` | 2256 | 1580 | 30% |
| `loop_sum` | 4513 | 3120 | 31% |
| `exception_handling` | 417 | 290 | 30% |

*Note: Benchmarks reflect interpretation overhead in a pure SageLang environment.*

## Documentation

- [Architecture](docs/ARCHITECTURE.md): Technical details of SVM and SRVM.
- [Specification](docs/SPEC.md): Formal specification of binary formats and execution.
- [RISC-V Design](docs/rv64.md): Deep dive into the SRVM register-based design.

## License

This project is licensed under the same terms as SageLang.
