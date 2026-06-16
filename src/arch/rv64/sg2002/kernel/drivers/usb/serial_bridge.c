// USB Serial (CDC-ACM) bridge
#include "serial_bridge.h"
#include <string.h>

#define SERIAL_BUFFER_SIZE 1024
static char tx_buffer[SERIAL_BUFFER_SIZE];
static int tx_head = 0;
static int tx_tail = 0;

void usb_serial_init() {
    // 1. Initialize USB PHY/OTG
    tx_head = 0;
    tx_tail = 0;
}

void usb_serial_write(const char* data, int len) {
    // 1. Buffer data for USB transmission
    for (int i = 0; i < len; i++) {
        tx_buffer[tx_tail] = data[i];
        tx_tail = (tx_tail + 1) % SERIAL_BUFFER_SIZE;
    }
}
