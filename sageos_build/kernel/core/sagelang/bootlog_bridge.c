#include "metal_vm.h"
#include "bootlog.h"
#include "bootinfo.h"

// Native function: bootlog_init(void)
// This will initialize the bootlog using the current boot info.
// We expect the boot info to be accessible to the kernel kernel module.
extern SageOSBootInfo* get_kernel_boot_info(void);

static MetalValue native_bootlog_init(MetalVM* vm, MetalValue* args, int argc) {
    bootlog_init(get_kernel_boot_info());
    return mv_nil();
}

// Native function: bootlog(msg)
static MetalValue native_bootlog(MetalVM* vm, MetalValue* args, int argc) {
    if (argc < 1 || args[0].type != MV_STR) return mv_nil();
    bootlog(metal_string_get(vm, args[0].as.str_idx));
    return mv_nil();
}

void register_bootlog_native_bindings(MetalVM* vm) {
    metal_vm_register_native(vm, "bootlog_init", native_bootlog_init);
    metal_vm_register_native(vm, "bootlog", native_bootlog);
}
