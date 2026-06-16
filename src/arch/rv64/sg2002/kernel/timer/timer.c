// Timer handling for SG2002
#include "sbi.h"

void sg2002_timer_init(unsigned long long interval) {
    // Set initial timer
    sbi_set_timer(interval);
}

void sg2002_handle_timer_interrupt() {
    // 1. Reschedule next timer interrupt
    // 2. Trigger scheduler tick
}
