#pragma once
#include "metal_vm.h"
void sage_shell_run(void);
void sage_init_run(void);
void register_natives(MetalVM *vm);
void bridge_write_char(char c);
int bridge_read_char(void);
