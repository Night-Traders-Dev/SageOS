#include "metal_vm.h"
#include "status.h"
#include "timer.h"
#include "battery.h"

// Native function: status_refresh(void)
static MetalValue native_status_refresh(MetalVM* vm, MetalValue* args, int argc) {
    (void)vm; (void)args; (void)argc;
    status_refresh();
    return mv_nil();
}

// Native function: ram_total_bytes(void)
static MetalValue native_ram_total_bytes(MetalVM* vm, MetalValue* args, int argc) {
    (void)vm; (void)args; (void)argc;
    return mv_num((double)status_ram_total());
}

// Native function: ram_used_bytes(void)
static MetalValue native_ram_used_bytes(MetalVM* vm, MetalValue* args, int argc) {
    (void)vm; (void)args; (void)argc;
    return mv_num((double)status_ram_used());
}

void register_status_native_bindings(MetalVM* vm) {
    metal_vm_register_native(vm, "status_refresh", native_status_refresh);
    metal_vm_register_native(vm, "ram_total_bytes", native_ram_total_bytes);
    metal_vm_register_native(vm, "ram_used_bytes", native_ram_used_bytes);
}
