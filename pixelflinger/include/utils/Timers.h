#ifndef PIXELFLINGER_SHIM_UTILS_TIMERS_H
#define PIXELFLINGER_SHIM_UTILS_TIMERS_H

#include <stdint.h>

typedef int64_t nsecs_t;

enum {
    SYSTEM_TIME_REALTIME  = 0,
    SYSTEM_TIME_MONOTONIC = 1,
    SYSTEM_TIME_PROCESS   = 2,
    SYSTEM_TIME_THREAD    = 3
};

static inline nsecs_t systemTime(int /*clock*/ = SYSTEM_TIME_MONOTONIC)
{
    return 0;
}

#endif /* PIXELFLINGER_SHIM_UTILS_TIMERS_H */
