/*
 * sage_shell_entry.c
 * C shim: registers OS native callbacks into MetalVM, loads pre-compiled
 * sage_shell bytecode, and runs the SageLang shell.
 *
 * NOTE: As of v0.2.0, the main shell entry is moving to the stable SageLang
 * runtime in core/sagelang/runtime.c. This file remains to provide 
 * backward compatibility for components still using the legacy MetalVM.
 */

#include <stdint.h>
#include <stddef.h>
#include "metal_vm.h"
#include "console.h"
#include "keyboard.h"
#include "ramfs.h"
#include "fat32.h"
#include "power.h"
#include "swap.h"
#include "sysinfo.h"
#include "status.h"
#include "timer.h"
#include "acpi.h"
#include "smp.h"
#include "battery.h"
#include "sysinfo.h"
#include "serial.h"
#include "pci.h"
#include "sdhci.h"
#include "shell.h"
#include "elf.h"
#include "dmesg.h"
#include "scheduler.h"
#include "version.h"
#include "dmesg.h"
#include "bootinfo.h"
#include "sage_shell_entry.h"

// System Init Bridge
void sage_init_run(void) {
    extern void sage_run_file(const char *path);
    if (vfs_stat("/etc/init.sage", NULL) == 0) {
        sage_run_file("/etc/init.sage");
    } else {
        console_write("\nWARN: /etc/init.sage not found, skipping init system.\n");
    }
}
