/* symbian_rt.c - Freestanding ARM EABI & C99 runtime helpers for Symbian EKA2.
 *
 * Routes aligned word copies and zero-fills directly to Symbian's XIP ROM
 * euser.dll cache-line burst routines (wordmove / memmove / memclr), and
 * eliminates any dependency on estlib.dll or drtaeabi.dll.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "symbian_os.h"
#include "gerbera/egl/eglInternal.h"

void sym_OnThreadDetach(void)
{
    void *tls = sym_DllTls();
    if ((uintptr_t)tls > 3u)
        egliDestroyState();
    else if (tls != NULL)
        sym_DllFreeTls();
}

void abort(void)
{
    sym_User_Invariant();
}

/* --- Fast memory primitives backed by euser ROM routines ----------------- */

void *memcpy(void *dst, const void *src, size_t n)
{
    if (n == 0u || dst == src)
        return dst;
    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)n) & 3u) == 0u)
        return sym_wordmove(dst, src, (unsigned int)n);
    return sym_memmove(dst, src, (unsigned int)n);
}

void *memmove(void *dst, const void *src, size_t n)
{
    if (n == 0u || dst == src)
        return dst;
    /* Note: euser wordmove handles overlapping regions safely on EKA2? No:
     * wordmove copies forward only! Only use wordmove when non-overlapping
     * or dst < src; otherwise use sym_memmove. */
    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)n) & 3u) == 0u &&
        (uintptr_t)dst < (uintptr_t)src)
    {
        return sym_wordmove(dst, src, (unsigned int)n);
    }
    return sym_memmove(dst, src, (unsigned int)n);
}

void *memset(void *s, int c, size_t n)
{
    unsigned char byte = (unsigned char)c;
    unsigned char *p;
    uint32_t w;

    if (n == 0u)
        return s;
    if (byte == 0u)
    {
        sym_memclr(s, (unsigned int)n);
        return s;
    }

    p = (unsigned char *)s;
    while (n != 0u && ((uintptr_t)p & 3u) != 0u)
    {
        *p++ = byte;
        --n;
    }

    w = (uint32_t)byte * 0x01010101u;
    if (n >= 16u)
    {
        uint32_t *wp = (uint32_t *)p;
        size_t blocks = n >> 4;
        n &= 15u;
        do
        {
            wp[0] = w;
            wp[1] = w;
            wp[2] = w;
            wp[3] = w;
            wp += 4;
        } while (--blocks != 0u);
        p = (unsigned char *)wp;
    }

    while (n >= 4u)
    {
        *(uint32_t *)p = w;
        p += 4;
        n -= 4u;
    }
    while (n-- != 0u)
        *p++ = byte;

    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n-- != 0u)
    {
        int diff = (int)*a++ - (int)*b++;
        if (diff != 0)
            return diff;
    }
    return 0;
}

int strcmp(const char *s1, const char *s2)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    for (;;)
    {
        unsigned char ca = *a++;
        unsigned char cb = *b++;
        if (ca != cb || ca == 0u)
            return (int)ca - (int)cb;
    }
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p != '\0')
        ++p;
    return (size_t)(p - s);
}

/* --- ARM EABI RTABI memory entry points (IHI0043D) ------------------------ */

void __aeabi_memcpy(void *dst, const void *src, size_t n)
{
    if (n != 0u && dst != src)
        sym_memmove(dst, src, (unsigned int)n);
}

void __aeabi_memcpy4(void *dst, const void *src, size_t n)
{
    if (n == 0u || dst == src)
        return;
    if ((n & 3u) == 0u)
        sym_wordmove(dst, src, (unsigned int)n);
    else
        sym_memmove(dst, src, (unsigned int)n);
}

void __aeabi_memcpy8(void *dst, const void *src, size_t n)
{
    __aeabi_memcpy4(dst, src, n);
}

void __aeabi_memmove(void *dst, const void *src, size_t n)
{
    (void)memmove(dst, src, n);
}

void __aeabi_memmove4(void *dst, const void *src, size_t n)
{
    (void)memmove(dst, src, n);
}

void __aeabi_memmove8(void *dst, const void *src, size_t n)
{
    (void)memmove(dst, src, n);
}

void __aeabi_memclr(void *dst, size_t n)
{
    if (n != 0u)
        sym_memclr(dst, (unsigned int)n);
}

void __aeabi_memclr4(void *dst, size_t n)
{
    if (n != 0u)
        sym_memclr(dst, (unsigned int)n);
}

void __aeabi_memclr8(void *dst, size_t n)
{
    if (n != 0u)
        sym_memclr(dst, (unsigned int)n);
}

/* Note ARM RTABI argument order: (void *dst, size_t n, int c)! */
void __aeabi_memset(void *dst, size_t n, int c)
{
    (void)memset(dst, c, n);
}

void __aeabi_memset4(void *dst, size_t n, int c)
{
    (void)memset(dst, c, n);
}

void __aeabi_memset8(void *dst, size_t n, int c)
{
    (void)memset(dst, c, n);
}
