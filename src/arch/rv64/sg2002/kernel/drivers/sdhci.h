#ifndef SDHCI_H
#define SDHCI_H

void sdhci_init();
int sdhci_read_block(unsigned long block_addr, void* buffer);
int sdhci_write_block(unsigned long block_addr, const void* buffer);

#endif
