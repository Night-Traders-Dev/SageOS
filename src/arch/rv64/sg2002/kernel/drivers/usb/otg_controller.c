// SG2002 USB OTG (DWC2) Driver
#include <stdint.h>

#define USB_OTG_BASE 0x04340000

// DWC2 Register offsets
#define GRSTCTL    0x010
#define GINTSTS    0x014
#define GINTMSK    0x018
#define GCFG       0x008

static inline void dwc2_write_reg(uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(USB_OTG_BASE + offset) = val;
}

static inline uint32_t dwc2_read_reg(uint32_t offset) {
    return *(volatile uint32_t *)(USB_OTG_BASE + offset);
}

void usb_otg_init() {
    // 1. Soft Reset
    // Set Core Soft Reset bit
    dwc2_write_reg(GRSTCTL, 0x1);
    
    // Wait for reset to complete
    while (dwc2_read_reg(GRSTCTL) & 0x1);

    // 2. Set mode to Device (or Host, depending on requirements - setting to Device for OTG)
    // For DWC2, bit 0 in GCFG is typically for device mode
    uint32_t cfg = dwc2_read_reg(GCFG);
    cfg |= 0x1; 
    dwc2_write_reg(GCFG, cfg);
}

void usb_otg_process_interrupts() {
    uint32_t status = dwc2_read_reg(GINTSTS);
    // Handle interrupts...
    dwc2_write_reg(GINTSTS, status); // Clear
}
