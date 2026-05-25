#ifndef SAGE_RUNTIME_BRIDGE_H
#define SAGE_RUNTIME_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

// Bridge for the old kernel code to the new SageLang runtime
void sage_execute(const char* line);
void sage_run_file(const char* path);
void sage_import_module(const char* name);
void sage_repl_init(void);

#endif
