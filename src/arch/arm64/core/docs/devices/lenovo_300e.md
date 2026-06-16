# Device Support: Lenovo 300e Chromebook (2nd Gen AST)

The Lenovo 300e Chromebook (2nd Gen AST) is a rugged convertible laptop based on the AMD Stoney Ridge architecture.

## Specifications
- **SoC**: AMD A4-9120C (Stoney Ridge).
- **CPU**: Dual-core x86_64.
- **GPU**: AMD Radeon R4 Graphics.
- **RAM**: 4GB or 8GB LPDDR4.
- **Firmware**: Coreboot with Tianocore (UEFI) payload.

## SageOS Implementation
The `300e` port of SageOS includes specific drivers for:
- **I2C Touchpad/Touchscreen**: Elan/Synaptics.
- **Keyboard**: ChromeOS-specific keyboard matrix.
- **Power Management**: ChromeOS EC (Embedded Controller) integration.

## Boot Procedure
1.  System boots into Tianocore UEFI.
2.  Tianocore loads `BOOTX64.EFI` from the EFI System Partition (ESP).
3.  SageOS kernel is loaded into high memory.
4.  Standard x86_64 Long Mode entry.

## Build Action
```bash
./sageos.sh x64 lenovo_300e build
```

## Running in QEMU
The `lenovo_300e` target can be emulated using the `q35` machine with specific CPU flags:
```bash
./sageos.sh x64 lenovo_300e run
```
Note: Full hardware emulation (EC, I2C) requires specific QEMU patches or configurations not yet included in the master script.
