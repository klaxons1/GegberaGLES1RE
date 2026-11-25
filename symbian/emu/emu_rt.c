/* emu_rt.c - bare-metal runtime for running the Symbian Gerbera core in an
 * ARM emulator (symbian/emu/run_emu.py, unicorn).
 *
 * The Gerbera core is compiled exactly like the Symbian DLL (same compiler,
 * -D__SYMBIAN32__, same flags), so it calls a handful of euser.dll
 * imports; they are provided here. Services the host must perform (console
 * output, timing markers, frame dumps, exit) are SVC calls the runner traps:
 *   svc #1  write(r0 = text, r1 = length)
 *   svc #2  exit(r0 = status)
 *   svc #3  marker(r0 = id, r1 = a, r2 = b)      timing / bookkeeping
 *   svc #4  instruction-memory barrier(r0 = start, r1 = end)
 *   svc #5  frame(r0 = pixels, r1 = width, r2 = height, r3 = tag)
 */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "emu_rt.h"

static inline int emu_svc(int number, int a, int b, int c, int d)
{
    register int r0 __asm__("r0") = a;
    register int r1 __asm__("r1") = b;
    register int r2 __asm__("r2") = c;
    register int r3 __asm__("r3") = d;
    switch (number)
    {
    case 1: __asm__ volatile("svc #1" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory"); break;
    case 2: __asm__ volatile("svc #2" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory"); break;
    case 3: __asm__ volatile("svc #3" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory"); break;
    case 4: __asm__ volatile("svc #4" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory"); break;
    default: __asm__ volatile("svc #5" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory"); break;
    }
    return r0;
}

void emu_exit(int status)
{
    emu_svc(2, status, 0, 0, 0);
    for (;;)
        ;
}

void emu_marker(int id, int a, int b) { emu_svc(3, id, a, b, 0); }
void emu_frame(const void *pixels, int width, int height, int tag)
{
    emu_svc(5, (int)(uintptr_t)pixels, width, height, tag);
}

/* --- C library subset ---------------------------------------------------- */

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if ((((uintptr_t)d | (uintptr_t)s | n) & 3u) == 0)
    {
        uint32_t *dw = (uint32_t *)d;
        const uint32_t *sw = (const uint32_t *)s;
        size_t i;
        for (i = 0; i < n / 4; ++i)
            dw[i] = sw[i];
        return dst;
    }
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d == s || n == 0)
        return dst;
    if (d < s)
        return memcpy(dst, src, n);
    while (n--)
        d[n] = s[n];
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    if ((((uintptr_t)d | n) & 3u) == 0)
    {
        uint32_t v = (unsigned char)c * 0x01010101u;
        uint32_t *w = (uint32_t *)d;
        size_t i;
        for (i = 0; i < n / 4; ++i)
            w[i] = v;
        return dst;
    }
    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; --n, ++x, ++y)
        if (*x != *y)
            return *x - *y;
    return 0;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
        ++a, ++b;
    return (unsigned char)*a - (unsigned char)*b;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        ++n;
    return n;
}

void __aeabi_memcpy(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memmove(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove4(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove8(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memset(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset8(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memclr(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr8(void *d, size_t n) { memset(d, 0, n); }

/* Heap: a bump allocator with size headers, enough for the benchmark run.
 * free() keeps a small exact-size free list so per-frame churn is bounded. */
extern char __heap_start[];
extern char __heap_end[];
static char *s_heapTop;
#define EMU_FREE_BUCKETS 64
static struct EmuBlock { struct EmuBlock *next; } *s_free[EMU_FREE_BUCKETS];

static size_t emu_round(size_t n) { return (n + 15u) & ~(size_t)15u; }

void *malloc(size_t n)
{
    size_t size = emu_round(n ? n : 1);
    size_t *p;
    if (size / 16 < EMU_FREE_BUCKETS && s_free[size / 16] != NULL)
    {
        struct EmuBlock *b = s_free[size / 16];
        s_free[size / 16] = b->next;
        return b;
    }
    if (s_heapTop == NULL)
        s_heapTop = __heap_start;
    if ((size_t)(__heap_end - s_heapTop) < size + 16)
    {
        emu_print("emu: out of memory\n");
        emu_exit(99);
    }
    p = (size_t *)(s_heapTop + 12);
    p[-1] = size;
    s_heapTop += size + 16;
    return p;
}

void free(void *ptr)
{
    size_t size;
    if (ptr == NULL)
        return;
    size = ((size_t *)ptr)[-1];
    if (size / 16 < EMU_FREE_BUCKETS)
    {
        struct EmuBlock *b = ptr;
        b->next = s_free[size / 16];
        s_free[size / 16] = b;
    }
}

void *calloc(size_t a, size_t b)
{
    void *p = malloc(a * b);
    memset(p, 0, emu_round(a * b));
    return p;
}

void *realloc(void *ptr, size_t n)
{
    void *q;
    size_t old;
    if (ptr == NULL)
        return malloc(n);
    old = ((size_t *)ptr)[-1];
    if (n <= old)
        return ptr;
    q = malloc(n);
    memcpy(q, ptr, old);
    free(ptr);
    return q;
}

void abort(void)
{
    emu_print("emu: abort\n");
    emu_exit(98);
}

/* --- Formatted output (subset: %d %u %x %s %c with width/zero pad) ------- */

static void emu_put(char *buf, int *len, int cap, char c)
{
    if (*len < cap - 1)
        buf[(*len)++] = c;
}

static void emu_num(char *buf, int *len, int cap, unsigned int v, int base,
                    int neg, int width, int zero)
{
    char tmp[16];
    int n = 0;
    do
    {
        tmp[n++] = "0123456789abcdef"[v % (unsigned)base];
        v /= (unsigned)base;
    } while (v);
    if (neg)
        tmp[n++] = '-';
    while (n < width)
        tmp[n++] = zero ? '0' : ' ';
    while (n)
        emu_put(buf, len, cap, tmp[--n]);
}

int emu_vformat(char *buf, int cap, const char *fmt, va_list ap)
{
    int len = 0;
    for (; *fmt; ++fmt)
    {
        int width = 0, zero = 0;
        if (*fmt != '%')
        {
            emu_put(buf, &len, cap, *fmt);
            continue;
        }
        ++fmt;
        if (*fmt == '0')
            zero = 1, ++fmt;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        switch (*fmt)
        {
        case 'd': { int v = va_arg(ap, int);
                    emu_num(buf, &len, cap, v < 0 ? 0u - (unsigned)v : (unsigned)v, 10, v < 0, width, zero); break; }
        case 'u': emu_num(buf, &len, cap, va_arg(ap, unsigned), 10, 0, width, zero); break;
        case 'x': emu_num(buf, &len, cap, va_arg(ap, unsigned), 16, 0, width, zero); break;
        case 'c': emu_put(buf, &len, cap, (char)va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *);
                    while (*s) emu_put(buf, &len, cap, *s++); break; }
        default: emu_put(buf, &len, cap, *fmt); break;
        }
    }
    buf[len] = 0;
    return len;
}

void emu_print(const char *text)
{
    emu_svc(1, (int)(uintptr_t)text, (int)strlen(text), 0, 0);
}

void emu_printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = emu_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    emu_svc(1, (int)(uintptr_t)buf, n, 0, 0);
}

/* --- euser.dll imports used by the Symbian-compiled core ---------------- */

void sym_User_IMB_Range(void *start, void *end)
{
    emu_svc(4, (int)(uintptr_t)start, (int)(uintptr_t)end, 0, 0);
}

void sym_User_Invariant(void)
{
    emu_print("emu: User::Invariant\n");
    emu_exit(97);
}

void *sym_memclr(void *trg, unsigned int len) { return memset(trg, 0, len); }
void *sym_memmove(void *trg, const void *src, unsigned int len) { return memmove(trg, src, len); }
void *sym_wordmove(void *trg, const void *src, unsigned int len) { return memmove(trg, src, len); }
int sym_memcompare(const unsigned char *l, int ll, const unsigned char *r, int rl)
{
    int n = ll < rl ? ll : rl;
    int c = memcmp(l, r, (size_t)n);
    return c ? c : ll - rl;
}
void *sym_User_Alloc(int size) { return malloc((size_t)size); }
void *sym_User_AllocZ(int size) { return calloc(1, (size_t)size); }
void sym_User_Free(void *p) { free(p); }

/* grUtil.c's grPrintf and hgAssert use these on Symbian builds. */
void sym_User_Panic(const void *category, int reason)
{
    (void)category;
    emu_printf("emu: panic %d\n", reason);
    emu_exit(96);
}
