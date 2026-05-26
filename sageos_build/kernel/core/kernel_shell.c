#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "keyboard.h"

#define MAX_LINE 256

void sage_shell_run(void) {
    char line[MAX_LINE];
    int pos = 0;
    
    console_write("\nSageOS SageVM Shell v0.1\n");
    console_write("root@sageos:/# ");
    
    while(1) {
        KeyEvent ev;
        if (!keyboard_wait_event(&ev)) continue;
        if (!ev.pressed) continue;
        
        if (ev.ascii == '\r' || ev.ascii == '\n') {
            line[pos] = '\0';
            console_write("\n");
            
            if (pos > 0) {
                console_write("Command: ");
                console_write(line);
                console_write("\n");
            }
            
            pos = 0;
            console_write("root@sageos:/# ");
        } else if (ev.ascii == 8) { // Backspace
            if (pos > 0) {
                pos--;
                console_write("\b \b");
            }
        } else if (ev.ascii >= 32 && ev.ascii < 127) {
            if (pos < MAX_LINE - 1) {
                line[pos++] = (char)ev.ascii;
                console_putc((char)ev.ascii);
            }
        }
    }
}
