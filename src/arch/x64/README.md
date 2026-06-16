# SageOS_x64

This is the **SageOS_x64** port of SageOS.

## Repository Structure

- **main**: Contains the architecture-agnostic core components shared across all SageOS platforms.
- **Hardware Branches**: Specific hardware implementations and drivers live in dedicated branches.

### Target Branches
- **[300e](https://github.com/Night-Traders-Dev/SageOS_x64/tree/300e)**: Lenovo 300e Chromebook (2nd Gen AST).

## Booting on x86_64

The x86_64 port supports booting on standard PC hardware and QEMU (`q35` machine).

To build and run:
1.  Use the `os.boot.build` module in SageLang.
2.  Generate a bootable ELF or ISO image.

Example QEMU command:
```bash
qemu-system-x86_64 -machine q35 -m 128M -display none -serial stdio -kernel kernel.elf
```
