# SageContainer Guide: Isolated SageOS Environments

SageContainer provides a chroot-like, isolated environment for SageOS development and testing. It leverages Linux namespaces (via SageLang's `os.linux.namespace` module) to create a sandbox where the Sage Shell and user-space components can run natively on the host without the overhead of a full virtual machine.

## Prerequisites

- **Linux Host**: Namespaces are a Linux kernel feature.
- **Root/Sudo Privileges**: Creating namespaces (CLONE_NEWNS, etc.) requires administrative rights.
- **SageLang Runtime**: The `sage` interpreter must be built (`make` in `sageos_build/sage_lang/core`).

## Architecture Support

SageContainer supports multi-architecture root filesystems located in `SageRoot/`:
- `arm64`: AArch64 SageOS environment.
- `rv64`: RISC-V 64-bit SageOS environment.
- `x64`: x86_64 SageOS environment.

## Usage

### 1. Set Environment Paths
Ensure `SAGE_PATH` points to the SageLang standard library:
```bash
export SAGE_PATH=sageos_build/sage_lang/core/lib
```

### 2. Launch the Container
Use the `run_chroot.sage` script to enter the environment for a specific architecture.

**Example: Entering the x64 environment**
```bash
sudo -E ./sageos_build/sage_lang/core/sage scripts/run_chroot.sage x64
```
*Note: The `-E` flag preserves your environment variables (like `SAGE_PATH`) for the sudo command.*

## Internals

### Isolated Namespaces
The launcher isolates the following namespaces:
- **Mount**: Provides a private root filesystem (`SageRoot/<arch>`).
- **UTS**: Allows a private hostname (`sage-os-<arch>`).
- **PID**: Isolates process IDs (the container sees itself as PID 1).
- **IPC**: Isolates inter-process communication.

### RootFS Structure
The environment follows an FHS-compliant layout:
- `/bin/sage`: The SageLang interpreter.
- `/etc/sagelang/`: System configuration and `.sage` source files.
- `/lib/sagelang/`: SGVM bytecode and library files.
- `/proc`, `/sys`, `/dev`: Mounted specifically for the container.

## Development Workflow

1. **Modify Code**: Edit `.sage` files in `sageos_build/kernel/` or `sageos_build/sage_lang/core/lib/`.
2. **Re-populate**: Run `scripts/populate_rootfs.sh` with the target `ROOTFS` variable to update the `SageRoot` directory.
   ```bash
   ROOTFS=SageRoot/x64 bash scripts/populate_rootfs.sh
   ```
3. **Test**: Launch the container to verify changes in the Sage Shell.

## Cross-Architecture Testing
For `arm64` and `rv64` containers on an `x86_64` host, ensure `qemu-user-static` is installed and `binfmt_misc` is configured. This allows the host to execute the foreign architecture `sage` binary inside the container seamlessly.
