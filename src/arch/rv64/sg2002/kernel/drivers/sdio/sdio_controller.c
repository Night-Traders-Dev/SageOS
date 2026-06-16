// SG2002 SDIO Controller driver
#include "sdio_controller.h"
#include <stdint.h>

#define SDIO1_BASE 0x04320000
#define PINMUX_BASE 0x03001000
#define CLK_EN_BASE 0x03002000

static inline void reg_write(uint32_t base, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(base + offset) = val;
}

void sdio_init() {
    // 1. Enable Clock (Enable SDIO1 clock)
    reg_write(CLK_EN_BASE, 0x4, 0x1 << 16); // Hypothetical bit
    
    // 2. Configure Pinmux (Set pins to SDIO mode)
    reg_write(PINMUX_BASE, 0x0, 0x1); // Hypothetical pin setup
    
    // 3. Reset Controller (Soft Reset in SDIO1_BASE + 0x0)
    reg_write(SDIO1_BASE, 0x0, 0x1);
}

int sdio_send_command(unsigned int cmd, unsigned int arg) {
    // Basic command dispatch
    return 0;
}
