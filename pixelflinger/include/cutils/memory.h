#ifndef PIXELFLINGER_SHIM_CUTILS_MEMORY_H
#define PIXELFLINGER_SHIM_CUTILS_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Note: In Android cutils/memory.h, `size` is in BYTES (not element count). */
static inline void android_memset16(uint16_t *dst, uint16_t value, size_t size)
{
    size_t n = size >> 1;
    uint32_t pair;
    if (n == 0)
        return;
    if (((uintptr_t)dst & 2u) != 0) {
        *dst++ = value;
        --n;
    }
    pair = (uint32_t)value | ((uint32_t)value << 16);
    uint32_t *dst32 = (uint32_t *)dst;
    while (n >= 2) {
        *dst32++ = pair;
        n -= 2;
    }
    if (n != 0)
        *(uint16_t *)dst32 = value;
}

static inline void android_memset32(uint32_t *dst, uint32_t value, size_t size)
{
    size_t n = size >> 2;
    while (n >= 4) {
        dst[0] = value;
        dst[1] = value;
        dst[2] = value;
        dst[3] = value;
        dst += 4;
        n -= 4;
    }
    while (n-- > 0)
        *dst++ = value;
}

#ifdef __cplusplus
}
#endif

#endif /* PIXELFLINGER_SHIM_CUTILS_MEMORY_H */
