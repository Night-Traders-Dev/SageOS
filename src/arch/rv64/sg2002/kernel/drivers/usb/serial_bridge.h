#ifndef SERIAL_BRIDGE_H
#define SERIAL_BRIDGE_H

void usb_serial_init();
void usb_serial_write(const char* data, int len);

#endif
