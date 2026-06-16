#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t time_t;
typedef uint64_t clock_t;

// struct timespec is in sage_port.h

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t *tloc);
clock_t clock(void);

#ifndef _TIMESPEC_DEFINED
#define _TIMESPEC_DEFINED
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};
#endif

int nanosleep(const struct timespec *req, struct timespec *rem);

#endif
