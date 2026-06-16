#ifndef SDIO_CONTROLLER_H
#define SDIO_CONTROLLER_H

void sdio_init();
int sdio_send_command(unsigned int cmd, unsigned int arg);

#endif
