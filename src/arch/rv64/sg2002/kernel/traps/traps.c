// Minimal trap handler for SG2002
// This will eventually be expanded to handle page faults, interrupts, etc.

extern void sg2002_uart_putc(char c);

void print_hex(unsigned long val) {
    // Basic hex printer using UART
}

void handle_trap(unsigned long cause, unsigned long epc) {
    // Log basic trap information
    // Eventually, call specialized handlers based on cause
    
    // Placeholder: Loop forever on unexpected traps for debugging
    while(1);
}
