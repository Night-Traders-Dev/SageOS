# Device Support: Raspberry Pi 4 (RPi4)

The Raspberry Pi 4 is the flagship ARM64 target for SageOS.

## Specifications
- **SoC**: Broadcom BCM2711.
- **CPU**: Quad-core Cortex-A72 (ARM v8).
- **UART**: PL011 (Base: `0xFE201000`).

## Boot Procedure
1.  RPi4 firmware looks for `kernel8.img` on the SD card (FAT32 partition).
2.  SageOS kernel is loaded at `0x80000`.
3.  Boot stub (`boot.S`) checks the processor ID.
4.  Cores 1-3 are parked (`WFE` loop).
5.  Core 0 initializes the stack and jumps to `kmain`.

## Build Action
```bash
./sageos.sh arm64 rpi4 build
```

## Running in QEMU
```bash
./sageos.sh arm64 rpi4 run
```
Note: Uses `-machine raspi4b`.
