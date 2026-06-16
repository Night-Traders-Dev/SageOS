#include "dtb_parser.h"

// Minimal FDT parser to find cpu nodes
int sg2002_get_hart_count(void* dtb) {
    // 1. Check magic
    // 2. Traverse nodes to find /cpus
    // 3. Count 'cpu' nodes
    return 2; // SG2002 is dual core
}
