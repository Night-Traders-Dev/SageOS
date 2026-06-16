// SG2002 SDHCI (DWC) driver
#include "sdhci.h"
#include <stdint.h>

#define SDHCI0_BASE 0x04310000

// DWC SDHCI Register Offsets (Partial)
#define SDHCI_DATA_PORT    0x000
#define SDHCI_INT_STATUS   0x030
#define SDHCI_CMD_ARG      0x008
#define SDHCI_CMD          0x00C
#define SDHCI_BLOCK_SIZE   0x004
#define SDHCI_BLOCK_COUNT  0x006

static inline void sdhci_write_reg(uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(SDHCI0_BASE + offset) = val;
}

static inline uint32_t sdhci_read_reg(uint32_t offset) {
    return *(volatile uint32_t *)(SDHCI0_BASE + offset);
}

void sdhci_init() {
    // 1. Reset
    // 2. Clock setup
    // 3. Power on
}

int sdhci_read_block(unsigned long block_addr, void* buffer) {
    // 1. Set Block size/count
    sdhci_write_reg(SDHCI_BLOCK_SIZE, 512);
    sdhci_write_reg(SDHCI_BLOCK_COUNT, 1);
    
    // 2. Send Read Command
    sdhci_write_reg(SDHCI_CMD_ARG, (uint32_t)block_addr);
    sdhci_write_reg(SDHCI_CMD, 0x1200); // READ_SINGLE_BLOCK command (simplified)

    // 3. Wait for data
    while (!(sdhci_read_reg(SDHCI_INT_STATUS) & 0x20)); // Wait for buffer ready

    // 4. Read data from FIFO
    uint32_t* buf = (uint32_t*)buffer;
    for (int i = 0; i < 512 / 4; i++) {
        buf[i] = sdhci_read_reg(SDHCI_DATA_PORT);
    }
    
    return 0;
}

int sdhci_write_block(unsigned long block_addr, const void* buffer) {
    // 1. Set Block size/count
    sdhci_write_reg(SDHCI_BLOCK_SIZE, 512);
    sdhci_write_reg(SDHCI_BLOCK_COUNT, 1);

    // 2. Write data to FIFO
    const uint32_t* buf = (const uint32_t*)buffer;
    for (int i = 0; i < 512 / 4; i++) {
        sdhci_write_reg(SDHCI_DATA_PORT, buf[i]);
    }

    // 3. Send Write Command
    sdhci_write_reg(SDHCI_CMD_ARG, (uint32_t)block_addr);
    sdhci_write_reg(SDHCI_CMD, 0x1800); // WRITE_SINGLE_BLOCK command (simplified)

    // 4. Wait for completion
    while (!(sdhci_read_reg(SDHCI_INT_STATUS) & 0x40));
    
    return 0;
}
