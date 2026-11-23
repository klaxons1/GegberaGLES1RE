#ifndef PIXELFLINGER_SHIM_CUTILS_ATOMIC_H
#define PIXELFLINGER_SHIM_CUTILS_ATOMIC_H

#include <stdint.h>

static inline int32_t android_atomic_inc(volatile int32_t *addr)
{
#if defined(__SYMBIAN32__)
    int32_t old = *addr;
    *addr = old + 1;
    return old;
#else
    return __sync_fetch_and_add(addr, 1);
#endif
}

static inline int32_t android_atomic_dec(volatile int32_t *addr)
{
#if defined(__SYMBIAN32__)
    int32_t old = *addr;
    *addr = old - 1;
    return old;
#else
    return __sync_fetch_and_sub(addr, 1);
#endif
}

#endif /* PIXELFLINGER_SHIM_CUTILS_ATOMIC_H */
