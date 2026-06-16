# Changelog

All notable changes to this project will be documented in this file.

## [0.9.7] - 2026-06-16

### Added
- **100% Opcode Parity**: Achieved full synchronization with the authoritative `bytecode.h` from SageLang (89 opcodes total, 0-87 and 255).
- **Matrix Visualization**: Implemented native `math.printm` support in both the VM and compiler.
- **Mobile-Friendly Docs**: Improved the architecture graph visualization with dynamic resizing and touch support.

### Changed
- **OOP Engine Refactor**: Significantly improved `OP_CALL_METHOD` to correctly handle direct class method calls and inheritance.
- **Documentation Maintenance**: Updated `ARCHITECTURE.md` and `ROADMAP.md` to reflect current implementation status, removing legacy warnings.

### Fixed
- **Opcode Mapping Fix**: Corrected the critical `OP_RAISE` collision (now correctly mapped to 58, not 68).
- **Scope Persistence Bug**: Fixed a VM bug where `OP_RETURN` prematurely popped the global scope in top-level chunks.
- **Output Interference**: Silenced intrusive debug logs in `MetalVM.init` that caused test suite mismatches.

## [0.9.6] - 2026-06-15

### Added
- **Core Builtins**: Exposed common host-level functions (`clock`, `str`, `int`, `tonumber`, `len`, `print`, `range`, `type`) to the guest global scope.
- **Experimental Native Modules**: Added `gc` and `reflect` modules as experimental stubs in the native bridge.

### Fixed
- **Opcode Encoding Audit**: Flagged a critical encoding collision where `OP_RAISE` (58) is incorrectly mapped to 68 (shared with `OP_GPU_END_COMMANDS`) by the `sgvmc` compiler.
- **Compiler Instruction Gaps**: Identified missing implementation for `OP_MATH_PRINTM` (87) in the compiler's binary emission pass.
- **VM Control Flow Gaps**: Identified missing handler implementations for `OP_BREAK` and `OP_CONTINUE` in the `MetalVM` interpreter.

### Changed
- **Documentation Parity**: Synchronized technical specifications with the current implementation, including explicit runtime safety limits and updated native bridge coverage.

## [0.9.5] - 2026-06-14

### Added
- **Internal Security Limits**: Implemented mandatory runtime limits for stack depth (65,536), call depth (1,024), and exception handler nesting (1,024) to prevent Denial of Service (DoS).
- **CLI UX Standard**: Unified `-h/--help` and `-v/--version` flag support across `sgvm` and `sgvmc` with consistent formatting and version reporting.
- **ML Native Bridge**: Integrated `ml_native` module into the native delegation bridge.

### Fixed
- **Native Bridge Regressions**: Flagged missing support for `re` and `json` modules in the roadmap for future restoration.

## [0.9.4] - 2026-06-14

### Added
- **SageLang v3.7.7 Support**: Updated the core compiler submodule and synchronized VM logic with the latest language specifications.
- **AST Execution Bridge**: Implemented `OP_EXEC_AST_STMT` delegation to the host `sys.exec`, enabling execution of non-lowered code segments within the VM.
- **Expanded Native Library Access**: Broadened the delegation bridge in `OP_IMPORT` to include `json` and `re` (regex) modules from the host standard library.

### Changed
- **Compiler Refactoring**: Cleaned up the VM implementation, removing legacy `end` keywords and standardizing loop increment logic for better performance on modern Sage backends.

### Fixed
- **Critical Exception Handler Leak**: Fixed a major architectural bug where exception handlers were not properly scoped to their call frames, leading to "leaked" handlers that erroneously caught exceptions in parent frames after a function returned.
- **Stack & Scope Unwinding**: Improved `OP_RAISE` to perform a full unwinding of the call stack, local scopes, and operand stack to correctly restore the VM state at the catch site.
- **Opcode Documentation Parity**: Performed a comprehensive audit and synchronization of the documentation website's opcode tables against the core implementation.

## [0.9.3] - 2026-06-09

### Added
- **Opcodes constants mapping**: Refactored interpreter's `run_step()` loop from magic numbers to named `OP_*` opcode constants.

### Fixed
- **Loop control safety**: Implemented explicit unexpected loop control error handling for `OP_BREAK` and `OP_CONTINUE` opcodes to match C VM behavior.
- **Delegation bridge stack safety**: Expanded dynamic guest-to-host delegation call argument capacity up to 8 arguments and added fallback checks to prevent stack corruption on limit violations.
- **Loader bounds checking**: Added robust bounds checking assertions when parsing headers, constants, string lengths, and chunk sizes to prevent out-of-bounds crashes on malformed/truncated binaries.

### Changed
- **Compiler Refactoring**: Restructured compiler's main entry point `compile()` into distinct helper methods (`first_pass()` and `second_pass()`) for a clean two-pass execution.

## [0.9.2] - 2026-06-08

### Added
- **Native Bridge Modules**: Re-implemented and exposed `re`, `ffi`, `mem`, `struct`, and `gc` native modules to guest VM.
- **AST Fallback**: Implemented `OP_EXEC_AST_STMT` to allow execution of non-lowered code via `sys.exec()`.

### Fixed
- **Execution Logic**: Robustly fixed chunk execution by scanning bytecode to correctly identify and skip function chunks.
- **VM Fall-through**: Added missing `return true` handlers to `OP_CONSTANT`, `OP_NIL`, `OP_TRUE`, `OP_FALSE`, `OP_POP`, `OP_PRINT`, `OP_RETURN`, and `OP_HALT` to ensure proper instruction completion.
- **Bytecode Serialization**: Reordered binary blob construction in the compiler to write constants before the main code, aligning with `MetalVM` loader expectations.

## [0.9.1] - 2026-06-08

### Added
- **Delegation Bridge**: Implemented a "Guest-to-Host" bridge allowing SageVM to delegate GPU opcodes, native imports, and complex calls directly to the host SageLang interpreter.
- **Unified Build System (SageMake)**: Replaced legacy `build.sh` with a modern, Python-based `sagemake` orchestrator using `rich` for visual feedback.
- **Host Call API**: Integrated `sys.call` for dynamic invocation of host native functions from the guest VM.

### Fixed
- **Opcode Stability**: Converted opcode definitions to integer literals to ensure exact matching and reliable execution across all backends.
- **Indentation Syntax**: Final cleanup of legacy `end` keywords and one-liner syntax to strictly adhere to SageLang's indentation-based blocks.

## [0.9.0] - 2026-06-08

### Added
- **Full Opcode Parity**: Synchronized all opcodes with SageLang v3.7.6, including support for classes, inheritance, exceptions, and GPU hot-path stubs.
- **OOP Engine**: Implemented native class instantiation, method dispatch, and attribute access in `sgvm_vm.sage`.
- **Exception Handling**: Full support for `try/catch/finally` and `raise` in the VM.
- **Advanced Data Structures**: Added native support for `slice`, `tuple`, and complex `dict` operations.
- **Flexible Compiler**: Upgraded `sgvmc.sage` to handle variable-length operands (1-4 bytes) and automatic `.sage` -> `.svm` -> `.sgvm` translation.

### Changed
- **Indentation-based Syntax**: Refactored all tools to strictly follow SageLang's indentation-based blocks, removing legacy `end` keywords.
- **Search Path Precedence**: Updated module loader to prioritize local `lib/` directory relative to the executable for better development workflow.

### Fixed
- **Builtin Mapping**: Fixed AOT compilation issues by using name-based builtin mapping for native host functions.
- **Argument Mapping**: Corrected argument ordering and `self` injection for method calls and constructors.

### Removed
- **Cruft Cleanup**: Aggressively removed legacy `.svm`, `.sgvm` artifacts and old debug drafts (`run_step_draft.sage`, etc.).

## [0.8.1] - 2026-06-06

### Added
- **Native Bridging Layer**: Implemented a comprehensive bridge to the host SageLang standard library, providing high-performance access to `math`, `io`, `sys`, and `re` modules.
- **Bytecode Verifier**: Added mandatory pre-execution verification for constant references and jump targets in `MetalVM`.
- **Inheritance Support**: Implemented recursive method lookup in `OP_CALL_METHOD` to support class inheritance.
- **O(1) Constant Pool**: Switched to dictionary-based deduplication in the compiler for linear-time builds.
- **Native String Methods**: Added support for calling native `find`, `replace`, and `split` methods on string primitives.
- **Multi-threading Engine**: Implemented full `thread` module support with `spawn`, `join`, `mutex`, `lock`, `unlock`, and `sleep`.
- **Global Interpreter Lock (GIL)**: Added a host-mutex-backed GIL to serialize guest bytecode execution, with smart yielding during blocking calls.
- **FFI & Memory Bridge**: Added `ffi`, `mem`, and `struct` modules for low-level host interop and raw memory management.
- **Guest Sandboxing**: Added `safe_mode` and `ffi_enabled` flags to restrict access to sensitive native modules.
- **Resource Tracking**: Implemented guest-level memory allocation tracking and enforcement of memory limits via `mem.limit()`.
- **GC Control Primitives**: Exposed host garbage collector controls (`gc.collect()`, `gc.stats()`) to the guest VM.

### Fixed
- **Maximum Search Paths Error**: Resolved a critical interpreter error in SageLang by preventing duplicate search paths and increasing the limit to 64.
- **CLI Flag Parsing**: Fixed a bug in the SageLang interpreter where `-I` flags were misidentified as script paths.
- **Call Argument Ordering**: Fixed a VM bug where arguments were passed in reverse order to native functions and methods.
- **Import Binding**: Updated `OP_IMPORT` to correctly return module objects, allowing them to be assigned to global variables.
- **Function Returns**: Correctly pop return value and restore execution state in `run_func`, ensuring `OP_RETURN` works as expected.
- **Constant Table Limit**: Removed the 512-constant limit by dynamically sizing local-to-global mapping tables.

## [Unreleased] - 2026-06-04

### Added
- **Floating Point Support**: Implemented manual IEEE 754 packing and unpacking in pure SageLang for full 64-bit double support in the VM and compiler.
- **Exception Handling**: Added support for `try/catch/finally` blocks in `MetalVM`.
- **Dynamic Module Loading**: Implemented the `IMPORT` opcode to allow runtime loading of external `.sgvm` files.
- **Enhanced Flow Control**: Switched to relative jumps for better compatibility with the standard SageLang bytecode format.
- **Improved AOT Support**: Optimized tools to work seamlessly as compiled binaries.
- **Build System**: Introduced a `Makefile` for compiling `sgvm.sage` and `sgvmc.sage` into native binaries.

### Fixed
- **Compatibility**: Refactored tools to use global functions (e.g., `push()`, `pop()`, `dict_has()`) for better AOT backend compatibility.
- **String Processing**: Implemented robust manual string slicing and trimming to ensure consistent behavior across backends.
- **Argument Parsing**: Improved argument detection to handle both interpreted and compiled execution modes.
- **Module Shadowing**: Fixed an issue where `io.readbytes` was unavailable due to library shadowing.
