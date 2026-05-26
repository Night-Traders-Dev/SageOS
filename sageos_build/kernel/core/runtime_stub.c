#include <stdint.h>
#include <stddef.h>
#include "console.h"

// Minimalist runtime interface for the SageOS shell
void sage_shell_run(void) {
    console_write("\n[SageShell] Starting...\n");
    console_write("SageOS Shell (Minimalist Virt Mode)\n");
    console_write("> ");
    // Minimal loop for shell interaction
    while(1) {
        // Here we could add minimal character reading loop
        // but for now, just let it exist.
    }
}
