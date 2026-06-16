#include "platform_vm.h"
#include "../uart/uart.h"
#include "../timer/timer.h"
#include "../../../../sageos_build/sage_lang/core/src/vm/vm.h"
#include <string.h>

static platform_vm_interface_t* vm_interface;

// Stub for actual SD card reading - for now we load from a memory buffer
// Assume the bytecode is embedded in the kernel image for early testing
extern char _binary_hello_sgvm_start[];
extern char _binary_hello_sgvm_end[];

void sg2002_vm_init(platform_vm_interface_t* interface) {
    vm_interface = interface;
    // Initialize VM internal state
    vm_init();
}

void sg2002_vm_run(void* bytecode) {
    // 1. Load bytecode (simplified: passed directly)
    // 2. Start execution loop
    vm_run(bytecode);
}

void sg2002_vm_run_embedded() {
    sg2002_vm_run(_binary_hello_sgvm_start);
}
