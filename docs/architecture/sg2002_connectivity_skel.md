# Connectivity Skeletons for SG2002

This document tracks the initial implementation skeletons for LicheeRV Nano connectivity features.

## SDIO (Wi-Fi Foundation)
- Located at `arch/rv64/sg2002/kernel/drivers/sdio/`
- Intended to support AIC8800 Wi-Fi via SDIO.
- Base address for SDIO1: `0x04320000`.

## Serial-over-USB (Console)
- Located at `arch/rv64/sg2002/kernel/drivers/usb/`
- Intended to provide CDC-ACM serial bridge for Sage Shell access over USB.

## Build Integration
These drivers are integrated into the `riscv64` target build configuration in `build.toml`.
