#ifndef PLATFORM_VM_H
#define PLATFORM_VM_H

// Interface for VM to access platform services
typedef struct {
    void (*console_putc)(char c);
    void (*timer_set)(unsigned long long ticks);
    // ... other platform-specific syscalls
} platform_vm_interface_t;

void sg2002_vm_init(platform_vm_interface_t* interface);
void sg2002_vm_run(void* bytecode);

#endif
