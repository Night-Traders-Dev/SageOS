/* Milestone 3: File I/O Test for SageOS */
#include <stdint.h>

#define SYS_read       0
#define SYS_write      1
#define SYS_open       2
#define SYS_close      3
#define SYS_exit       60

long syscall1(long num, long a1) {
    long ret;
    __asm__ volatile ("mov x8, %1\nmov x0, %2\nsvc #0\nmov %0, x0" : "=r"(ret) : "r"(num), "r"(a1) : "x0", "x8", "memory");
    return ret;
}

long syscall3(long num, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("mov x8, %1\nmov x0, %2\nmov x1, %3\nmov x2, %4\nsvc #0\nmov %0, x0" : "=r"(ret) : "r"(num), "r"(a1), "r"(a2), "r"(a3) : "x0", "x1", "x2", "x8", "memory");
    return ret;
}

void _start() {
    const char *msg = "Milestone 3: Reading /etc/version...\n";
    syscall3(SYS_write, 1, (long)msg, 37);

    /* Open /etc/version */
    long fd = syscall3(SYS_open, (long)"/etc/version", 0, 0);
    if (fd < 0) {
        syscall3(SYS_write, 1, (long)"Error: Could not open /etc/version\n", 35);
        syscall3(SYS_exit, 1, 0, 0);
    }

    /* Read contents */
    char buffer[64];
    long bytes = syscall3(SYS_read, fd, (long)buffer, sizeof(buffer));
    
    if (bytes > 0) {
        syscall3(SYS_write, 1, (long)"Contents: ", 11);
        syscall3(SYS_write, 1, (long)buffer, bytes);
        syscall3(SYS_write, 1, (long)"\n", 1);
    } else {
        syscall3(SYS_write, 1, (long)"Error: Read failed or empty file\n", 33);
    }

    /* Close */
    syscall1(SYS_close, fd);

    syscall3(SYS_write, 1, (long)"File I/O test complete.\n", 24);
    syscall3(SYS_exit, 0, 0, 0);
}
