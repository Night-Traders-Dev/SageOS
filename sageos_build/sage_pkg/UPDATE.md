# SagePkg Updates

## [1.5.2] - 2026-05-16
### Security
- **SagePkg**: Mitigated potential command injection in `download_file` by introducing URL sanitization and stripping single quotes from input URLs.

### Fixed
- **SageNano**: Implemented robust terminal state restoration (`stty icanon echo`) on all exit points.
- **SageShell**: Implemented robust terminal state restoration (`stty icanon echo`) on all exit points, ensuring terminal reset even on abnormal exits.
- **SageShell**: Corrected syntax error in `main.sage` where `if` condition required newline separation for its body.

### Changed
- Incremented all package versions to 1.5.2 in `metadata.json`.

## [1.5.1] - 2026-05-15
### Security
- **SagePkg**: Added `is_safe_path()` validation for all file paths received from downloaded metadata, preventing path-traversal and shell-injection via malicious packages.
- **SagePkg**: All shell-constructed paths now wrapped in single quotes in `_sys.exec()` calls.
- **SagePkg**: `cmd_remove` now resolves and verifies full paths via `readlink -f` before running `rm -rf`, guarding against empty-HOME expansion.

### Fixed
- **SagePkg**: Replaced hardcoded `"1.1.2"` fallback version in `cmd_update` with `SAGEPKG_VERSION` constant — no more stale self-update detection.
- **SagePkg**: Fixed duplicate update entry for `sagepkg` in `cmd_update`; bootstrap-case check now only runs when `sagepkg` is absent from `installed.json`.
- **SagePkg**: Replaced shared static `TEMP_FILE` with a per-process PID-scoped path to eliminate race conditions between concurrent invocations.
- **SagePkg**: Extra-binary wrappers (e.g. for `SageUtils`) now inject the binary name as `args[2]` so `main.sage` can route by command without fragile basename-of-script-path detection.
- **SageUtils**: Rewrote `main()` routing to consume the injected command name at `args[2]`; fixes broken dispatch when called via sagepkg-generated wrappers.
- **SageFetch**: Added full ARM CPU part-number lookup table (Cortex-A35 through Cortex-X4, Qualcomm Kryo, Apple Silicon) replacing the raw `"ARM Part 0xNNN"` fallback.
- **SageShell**: Changed `IS_TTY` to be set via an explicit `check_tty()` call rather than an implicit module-load side-effect, making the detection intent clear.
- **Validation**: `scripts/validate.sage` now physically checks that every file declared in `metadata.json` exists in the repository using `test -f`.

### Changed
- **SagePkg**: `sagepkg.sage` root duplicate removed; `install.sh` now installs directly from `packages/sagepkg/universal/sagepkg.sage`.
- **SagePkg**: `lib/json.sage` introduced as the single canonical source for the JSON library. Root `json.sage` and `packages/sagepkg/universal/json.sage` are synced copies. Run `make sync-deps` after editing `lib/json.sage`.
- **CI**: Added sync-verification step to ensure bundled `json.sage` copies match `lib/json.sage`.
- **packages.json**: Bumped `sagepkg` version to `1.5.1` (was stale at `1.1.2`). Added architecture list for `SageUtils`.

## [1.5.0] - 2026-05-15
### Changed
- **SageFetch**: Switched to script-only mode (v1.1.0) to resolve `SIGSEGV` issues with outdated binaries.
- **SageShell**: Switched to script-only mode (v1.2.0) for better stability and consistent terminal behavior.
- **SagePkg**: Improved package installation logic to handle script wrappers more efficiently.

## [1.4.0] - 2026-05-15
### Added
- **SagePkg**: Bundled `json.sage` dependency to ensure cross-platform compatibility.
- **SageShell**: Fixed missing imports and refactored to use standard `sys` and `io` modules.
- **SageShell**: Fixed SageLang REPL support by ensuring correct terminal state and execution.
- **SageFetch**: Fixed missing imports and refactored to use standard `sys` and `io` modules.
- **SageFetch**: Improved OS, Kernel, and Hardware detection logic.

## [1.3.0] - 2026-05-15
### Added
- **SagePkg**: Support for `riscv64` architecture.
- **SagePkg**: Improved reliability of architecture and path detection.
- **SagePkg**: Dynamic installation dates for packages.
- **SagePkg**: Input validation for package names to prevent command injection.
- **SagePkg**: Better error reporting for network and compilation failures.

## [1.2.0] - 2026-05-13
### Added
- **SagePkg**: New `build` command to force compilation from source.
- **SagePkg**: New `remove` command to uninstall packages.
- **SagePkg**: Automatic fallback to building from source if binary is missing during `install`.

## [1.1.0] - 2026-05-13
### Added
- **SageShell**: Support for keyboard key combos (Ctrl+L, Ctrl+D, Ctrl+C).
- **SageShell**: Persistent command history via Up/Down arrows.
- **SageShell**: New prompt using the Sage emoji 🌿.
- **SagePkg**: Support for universal source code directory (`universal/`).
- **SagePkg**: Architecture-specific binaries for `aarch64`.

### Changed
- Refactored package structure to separate source (`universal/`) and binaries.
- Updated metadata versioning for all packages.
- Optimized SageFetch for faster execution.

### Fixed
- Improved robustness of terminal output handling in SageShell.
- Fixed character deletion and cursor movement in custom input loop.
