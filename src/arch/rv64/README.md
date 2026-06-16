# SageOS_rv64

This is the **SageOS_rv64** port of SageOS.

## Repository Structure

- **main**: Contains the architecture-agnostic core components shared across all SageOS platforms.
- **Hardware Branches**: Specific hardware implementations and drivers live in dedicated branches.

### Target Branches
- **[OrangePi_RV_2](https://github.com/Night-Traders-Dev/SageOS_rv64/tree/OrangePi_RV_2)**: Orange Pi RV 2.

## Booting on RISC-V 64

The RV64 port supports booting on RISC-V QEMU (`virt` machine).

To build and run:
1.  Use the `os.boot.build` module in SageLang.
2.  The build pipeline generates a flat binary for OpenSBI or direct kernel loading.

Example QEMU command:
```bash
qemu-system-riscv64 -machine virt -m 128M -display none -serial stdio -bios none -kernel kernel.elf
```
