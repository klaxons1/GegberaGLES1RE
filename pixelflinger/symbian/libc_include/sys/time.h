#ifndef PF_SYMBIAN_SYS_TIME_H
#define PF_SYMBIAN_SYS_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

struct timeval {
    long tv_sec;
    long tv_usec;
};

static inline int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
