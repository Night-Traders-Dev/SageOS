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
    // Early debug
    volatile uint8_t *uart = (volatile uint8_t *)0x10000000;
    uart[0] = 'D';
    uart[0] = 'E';
    uart[0] = 'B';
    uart[0] = 'U';
    uart[0] = 'G';
    uart[0] = '\n';

    // 1. Initialize hardware (Serial & Console)
    console_init(info);
    
    // Simple loop for early visual debug (if needed, this can be removed)
    // for (volatile int i=0; i<1000000; i++);

    // This message is already there, let's see if we get to it.
    console_write("\n[DEBUG] console_init finished\n");
    
    console_write("\n\033[1;36mSageOS Kernel (Virt) starting...\033[0m\n");
    console_write("Version: "); console_write(SAGEOS_VERSION); console_write("\n");
    
    // 3. Launch Shell
    console_write("Launching C Shell...\n");
    shell_run();

    console_write("[DEBUG] shell_run() returned! Halting.\n");
    while(1);
    
    // If shell exits, halt
    console_write("\nSystem halted.\n");
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
