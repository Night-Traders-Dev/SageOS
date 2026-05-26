#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "bootinfo.h"
#include "version.h"

// External SageLang runtime init (dummy for now)
void sage_kernel_early_init(void) {}

void power_reboot(void) {

    console_write("Rebooting...\n");
    while (1) {
#if defined(__x86_64__)
        __asm__ volatile ("hlt");
#elif defined(__aarch64__)
        __asm__ volatile ("wfe");
#elif defined(__riscv)
        __asm__ volatile ("wfi");
#endif
    }
}

void power_shutdown(void) {
    console_write("Shutting down...\n");
    while (1) {
#if defined(__x86_64__)
        __asm__ volatile ("hlt");
#elif defined(__aarch64__)
        __asm__ volatile ("wfe");
#elif defined(__riscv)
        __asm__ volatile ("wfi");
#endif
    }
}

void kmain(SageOSBootInfo *info) {
    // Confirm reachability via UART
    volatile uint8_t *uart = (volatile uint8_t *)0x10000000;
    uart[0] = 'K';
    
    // Minimal spin
    while(1);
}
