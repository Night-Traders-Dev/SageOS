#include "metal_vm.h"
#include "io.h"
#include "console.h"
#include "scheduler.h"

// Define external kernel functions to be bridged
extern void ata_timer_tick(void);
extern void console_periodic_flip(void);

// Native function: outb(port, val)
static MetalValue native_outb(MetalVM* vm, MetalValue* args, int argc) {
    if (argc < 2) return mv_nil();
    uint16_t port = (uint16_t)args[0].as.num_bits;
    uint8_t val = (uint8_t)args[1].as.num_bits;
    outb(port, val);
    return mv_nil();
}

// Native function: console_periodic_flip()
static MetalValue native_console_periodic_flip(MetalVM* vm, MetalValue* args, int argc) {
    console_periodic_flip();
    return mv_nil();
}

// Native function: ata_timer_tick()
static MetalValue native_ata_timer_tick(MetalVM* vm, MetalValue* args, int argc) {
    ata_timer_tick();
    return mv_nil();
}

// Native function: sched_timer_tick()
static MetalValue native_sched_timer_tick(MetalVM* vm, MetalValue* args, int argc) {
    sched_timer_tick();
    return mv_nil();
}

void register_timer_native_bindings(MetalVM* vm) {
    metal_vm_register_native(vm, "outb", native_outb);
    metal_vm_register_native(vm, "console_periodic_flip", native_console_periodic_flip);
    metal_vm_register_native(vm, "ata_timer_tick", native_ata_timer_tick);
    metal_vm_register_native(vm, "sched_timer_tick", native_sched_timer_tick);
}
