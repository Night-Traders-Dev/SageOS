#ifndef SAGEOS_SERIAL_H
#define SAGEOS_SERIAL_H

/* UART Base Addresses */
#define UART_BASE_X64      0x3F8        /* COM1 Port */
#define UART_BASE_ARM64    0x09000000   /* QEMU Virt PL011 */
#define UART_BASE_RV64     0x10000000   /* QEMU Virt NS16550 */

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
int serial_poll_char(char *out);
void serial_process_tx_buffer(void);

#endif
