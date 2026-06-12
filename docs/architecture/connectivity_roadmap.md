# Wi-Fi & Connectivity Plan for LicheeRV Nano

This plan outlines the roadmap to implement Wi-Fi support (Option A) and a Serial-over-USB bridge (Option B) for the LicheeRV Nano.

## Connectivity Overview
- **Wi-Fi:** Based on the AIC8800 chip connected via SDIO.
- **Serial-over-USB:** A bridge to redirect kernel console/shell output over USB (CDC-ACM).

## Milestone A: Wi-Fi Support (AIC8800/SDIO)
1. **SDHCI/SDIO Controller Driver:** Implement the base SDIO driver using the SG2002 SDIO0/SDIO1 register map (`0x04310000`/`0x04320000`).
2. **AIC8800 Driver Integration:** Port the AIC8800 driver to SageOS, ensuring firmware loading support.
3. **Network Stack (LwIP):** Integrate the LwIP stack for network protocol support (TCP/IP).
4. **Shell over Network:** Implement a minimal TCP server to provide a remote Sage Shell.

## Milestone B: Serial-over-USB (Immediate Connectivity)
1. **USB Controller Driver:** Research SG2002 USB PHY/OTG controller support.
2. **CDC-ACM Class Driver:** Implement the USB CDC-ACM (Communication Device Class) protocol for virtual serial port support.
3. **Console Redirection:** Redirect the SageOS console output and input streams to the USB virtual serial port.

## Documentation
- `docs/architecture/sg2002_wifi.md` (To be created)
- `docs/architecture/sg2002_usb_serial.md` (To be created)
