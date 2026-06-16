# Release 0.4.5 - Toolchain Integration & VFS Refinement

SageOS 0.4.5 is a significant maintenance and feature release focused on completing the native toolchain integration and providing a more robust interactive environment.

## Key Changes

### Infrastructure & Toolchain
- **Automated Sage Recompilation**: The master build script now automatically recompiles Sage components (VFS bridge, shell) into bytecode during the kernel build process.
- **Flexible Toolchain Installation**: `install_toolchain.sh` now supports custom paths and automated downloads from GitHub for all virtual architectures.
- **Standardized Versioning**: Centralized version management in the root `VERSION` file, which now dynamically generates kernel headers.

### Filesystem (VFS) & Storage
- **FAT32 Long File Name (LFN) Support**: Full support for long filenames in the FAT32 driver, enabling access to complex toolchain directories and executables.
- **VFS Union Mounts**: Implemented union-style mount point merging. This allows multiple filesystems (like RamFS and FAT32) to overlay at the same path (e.g., `/`), presenting a unified view of the system.
- **Expanded Standard Layout**: Reorganized and expanded the initial directory structure to include `/usr/bin`, `/var`, `/home`, etc., following Unix standards.
- **Lowercase Filename Normalization**: Traditional 8.3 FAT32 names are now automatically presented in lowercase for a consistent CLI experience.

### User Interface & Input
- **ANSI Escape Sequence Support**: Implemented a terminal escape parser in the virtual keyboard driver.
- **Enhanced Shell Navigation**: Full support for arrow keys (history and cursor movement), Home, End, and Delete keys in both C and Sage shells.
- **Automated /bin Population**: Core Sage utilities are now automatically copied from `/etc/commands/` to `/bin/` during boot for easier access.

## Architecture Support
- Continued robust support for **x86_64**, **AArch64**, and **RISC-V 64** virt platforms.
