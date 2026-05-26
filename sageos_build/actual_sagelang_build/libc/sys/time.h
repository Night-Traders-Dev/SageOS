#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <time.h>

struct timeval {
    time_t tv_sec;
    long   tv_usec;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

int clock_gettime(int clk_id, struct timespec *tp);
int gettimeofday(struct timeval *tv, void *tz);

#endif
