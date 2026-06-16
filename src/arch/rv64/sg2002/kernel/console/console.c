#include "console.h"
#include "../uart/uart.h"
#include "../usb/serial_bridge.h"

static console_type_t current_console = CONSOLE_UART;

void console_init(console_type_t type) {
    current_console = type;
    if (type == CONSOLE_UART) {
        sg2002_uart_init();
    } else if (type == CONSOLE_USB) {
        usb_serial_init();
    }
}

void console_write(const char* data, int len) {
    if (current_console == CONSOLE_UART) {
        for (int i = 0; i < len; i++) sg2002_uart_putc(data[i]);
    } else if (current_console == CONSOLE_USB) {
        usb_serial_write(data, len);
    }
}
