#ifndef CONSOLE_H
#define CONSOLE_H

typedef enum {
    CONSOLE_UART,
    CONSOLE_USB
} console_type_t;

void console_init(console_type_t type);
void console_write(const char* data, int len);

#endif
