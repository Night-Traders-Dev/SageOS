/* Milestone 3+: Argument Passing Test for SageOS */
#include <stdint.h>
#include <stddef.h>

#define SYS_write      1
#define SYS_exit       60

long syscall3(long num, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("mov x8, %1\nmov x0, %2\nmov x1, %3\nmov x2, %4\nsvc #0\nmov %0, x0" : "=r"(ret) : "r"(num), "r"(a1), "r"(a2), "r"(a3) : "x0", "x1", "x2", "x8", "memory");
    return ret;
}

static size_t my_strlen(const char *s) {
    size_t l = 0;
    while (*s++) l++;
    return l;
}

void _start() {
    /* On entry, SP points to argc, then argv pointers */
    uint64_t *stack_ptr;
    __asm__ volatile ("mov %0, sp" : "=r"(stack_ptr));

    uint64_t argc = *stack_ptr;
    char **argv = (char **)(stack_ptr + 1);

    syscall3(SYS_write, 1, (long)"Argument Passing Test\n", 22);
    syscall3(SYS_write, 1, (long)"argc: ", 6);
    
    char c = (char)('0' + (argc % 10));
    syscall3(SYS_write, 1, (long)&c, 1);
    syscall3(SYS_write, 1, (long)"\n", 1);

    for (uint64_t i = 0; i < argc; i++) {
        syscall3(SYS_write, 1, (long)"argv[", 5);
        char idx = (char)('0' + (i % 10));
        syscall3(SYS_write, 1, (long)&idx, 1);
        syscall3(SYS_write, 1, (long)"]: ", 3);
        
        if (argv[i]) {
            syscall3(SYS_write, 1, (long)argv[i], my_strlen(argv[i]));
        } else {
            syscall3(SYS_write, 1, (long)"(null)", 6);
        }
        syscall3(SYS_write, 1, (long)"\n", 1);
    }

    syscall3(SYS_exit, 0, 0, 0);
}
