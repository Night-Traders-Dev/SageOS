#ifndef _SAGEOS_PHYS_ALLOC_H
#define _SAGEOS_PHYS_ALLOC_H

#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"

#define PAGE_SIZE 4096

void phys_init(SageOSBootInfo *info);
void* phys_alloc(void);
void phys_free(void *addr);

#endif
