#include "metal_vm.h"
#include "power.h"
#include "io.h"
#include "console.h"

// Native function: power_qemu_exit(void)
static MetalValue native_power_qemu_exit(MetalVM* vm, MetalValue* args, int argc) {
    (void)vm; (void)args; (void)argc;
    power_qemu_exit();
    return mv_nil();
}

// Native function: power_reboot(void)
static MetalValue native_power_reboot(MetalVM* vm, MetalValue* args, int argc) {
    (void)vm; (void)args; (void)argc;
    power_reboot();
    return mv_nil();
}

// Native function: power_shutdown(void)
static MetalValue native_power_shutdown(MetalVM* vm, MetalValue* args, int argc) {
    (void)vm; (void)args; (void)argc;
    power_shutdown();
    return mv_nil();
}

void register_power_native_bindings(MetalVM* vm) {
    metal_vm_register_native(vm, "power_qemu_exit", native_power_qemu_exit);
    metal_vm_register_native(vm, "power_reboot", native_power_reboot);
    metal_vm_register_native(vm, "power_shutdown", native_power_shutdown);
}
