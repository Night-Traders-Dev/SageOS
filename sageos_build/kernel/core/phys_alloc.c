#include "phys_alloc.h"
#include "dmesg.h"

#define MAX_PHYS_PAGES 1048576 /* 4GB support */
static uint8_t bitmap[MAX_PHYS_PAGES / 8];

static void mark_used(uint64_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static void mark_free(uint64_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static int is_used(uint64_t page) {
    return bitmap[page / 8] & (1 << (page % 8));
}

extern char __kernel_start[];
extern char __kernel_end[];

void phys_init(SageOSBootInfo *info) {
    /* Mark the first 1MB as used (BIOS/UEFI legacy / early pages) */
    for (uint64_t i = 0; i < 256; i++) {
        mark_used(i);
    }

    /* Mark all kernel pages as used */
    uint64_t start_page = (uint64_t)__kernel_start / PAGE_SIZE;
    uint64_t end_page = ((uint64_t)__kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = start_page; i < end_page; i++) {
        mark_used(i);
    }

    /* Mark backbuffer pages as used if present */
    if (info && info->backbuffer_address) {
        uint64_t bb_start = info->backbuffer_address / PAGE_SIZE;
        uint64_t bb_pages = 4096; /* 16MB */
        for (uint64_t i = bb_start; i < bb_start + bb_pages; i++) {
            mark_used(i);
        }
    }

    dmesg_log("phys_alloc: initialized");
}

void* phys_alloc(void) {
    for (uint64_t i = 0; i < MAX_PHYS_PAGES; i++) {
        if (!is_used(i)) {
            mark_used(i);
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void phys_free(void *addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    mark_free(page);
}
