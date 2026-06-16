# Device Support: QEMU Virt (ARM64)

The `virt` machine is a generic platform provided by QEMU, ideal for kernel development where specific hardware quirks are not the focus.

## Specifications
- **CPU**: Cortex-A57 (default) or Cortex-A72.
- **UART**: PL011 (Base: `0x09000000`).
- **Interrupts**: GICv2/v3.

## Boot Procedure
- QEMU loads the kernel ELF directly.
- Entry point defined in the ELF header (`_start`).
- UART is usually pre-initialized by QEMU, but SageOS performs a full init for consistency.

## Usage
```bash
./sageos.sh arm64 virt run
```
