#include "smp.h"
#include "sbi_hsm.h"
#include "dtb_parser.h"

extern void secondary_hart_entry(void);

void sg2002_boot_secondary_harts(void* dtb) {
    int num_harts = sg2002_get_hart_count(dtb);
    for (int i = 1; i < num_harts; i++) {
        // Boot secondary harts
        sbi_hsm_hart_start(i, (unsigned long)secondary_hart_entry, 0);
    }
}
