/* pf_symbian_rt.cpp - Zero-WSD C/C++ runtime, expandable ChunkHeap allocator,
 * RChunk executable JIT code store, and IEEE-754 math for Symbian OS EKA2.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <new>

#include "symbian_os.h"
#include "../libagl/context.h"

#define PF_HEAP_MAX_BYTES   (32 * 1024 * 1024)

struct PfSymbianTls {
    android::gl::ogles_context_t*   glContext;
    void*                           chunkHeap;
    int                             inInit;
    void*                           eglState;
    void                          (*eglDestroyFn)(void*);
};

PfSymbianTls* pf_symbian_tls_peek(void)
{
    return (PfSymbianTls*)sym_DllTls();
}

PfSymbianTls* pf_symbian_tls_ensure(void)
{
    PfSymbianTls* tls = (PfSymbianTls*)sym_DllTls();
    if (tls)
        return tls;

    tls = (PfSymbianTls*)sym_User_AllocZ((int)sizeof(PfSymbianTls));
    if (!tls)
        return 0;

    tls->inInit = 1;
    if (sym_DllSetTls(tls) != 0) {
        sym_User_Free(tls);
        return 0;
    }

    int maxRam = 0x1000000;
    if (sym_HAL_Get(SYM_HAL_MEMORY_RAM, &maxRam) != 0 || maxRam <= 0)
        maxRam = 0x1000000;
    if (maxRam > PF_HEAP_MAX_BYTES)
        maxRam = PF_HEAP_MAX_BYTES;

    /* Match proven Symbian EKA2 ChunkHeap parameters from Gerbera eglSymbian.c */
    tls->chunkHeap = sym_UserHeap_ChunkHeap(0, 0, maxRam, 1, 0, 0);

    tls->inInit = 0;
    return tls;
}

void pf_symbian_set_egl_state(void* state, void (*destroyFn)(void*))
{
    PfSymbianTls* tls = pf_symbian_tls_ensure();
    if (tls) {
        tls->eglState = state;
        tls->eglDestroyFn = destroyFn;
    }
}

void* pf_symbian_get_egl_state(void)
{
    PfSymbianTls* tls = (PfSymbianTls*)sym_DllTls();
    return tls ? tls->eglState : 0;
}

extern "C" void pf_symbian_on_thread_detach(void)
{
    PfSymbianTls* tls = (PfSymbianTls*)sym_DllTls();
    if (!tls)
        return;

    if (tls->eglState && tls->eglDestroyFn) {
        void* st = tls->eglState;
        void (*fn)(void*) = tls->eglDestroyFn;
        tls->eglState = 0;
        tls->eglDestroyFn = 0;
        fn(st);
    }

    void* heap = tls->chunkHeap;
    tls->chunkHeap = 0;
    if (heap)
        sym_RAllocator_Close(heap);

    sym_DllFreeTls();
    sym_User_Free(tls);
}

namespace android {
namespace gl {

void setGlThreadSpecific(ogles_context_t* value)
{
    PfSymbianTls* tls = value ? pf_symbian_tls_ensure() : pf_symbian_tls_peek();
    if (tls)
        tls->glContext = value;
}

ogles_context_t* getGlThreadSpecific()
{
    PfSymbianTls* tls = (PfSymbianTls*)sym_DllTls();
    return tls ? tls->glContext : 0;
}

} // namespace gl
} // namespace android

/* --- 8-byte-aligned heap allocator over expandable ChunkHeap -------------- */

struct PfAllocHeader {
    uint32_t size;
    void*    ownerHeap;
};

extern "C" void* malloc(size_t size)
{
    size_t payload = (size == 0) ? 4u : size;
    if (payload > 0x70000000u)
        return 0;

    size_t total = sizeof(PfAllocHeader) + payload;
    PfAllocHeader* hdr = 0;
    void* usedHeap = 0;

    PfSymbianTls* tls = (PfSymbianTls*)sym_DllTls();
    if (!tls)
        tls = pf_symbian_tls_ensure();

    if (tls && tls->chunkHeap) {
        hdr = (PfAllocHeader*)sym_RHeap_Alloc(tls->chunkHeap, (int)total);
        if (hdr)
            usedHeap = tls->chunkHeap;
    }
    if (!hdr) {
        hdr = (PfAllocHeader*)sym_User_Alloc((int)total);
        usedHeap = 0;
    }
    if (!hdr)
        return 0;

    hdr->size = (uint32_t)payload;
    hdr->ownerHeap = usedHeap;
    return (void*)(hdr + 1);
}

extern "C" void free(void* ptr)
{
    if (!ptr)
        return;
    PfAllocHeader* hdr = ((PfAllocHeader*)ptr) - 1;
    if (hdr->ownerHeap)
        sym_RHeap_Free(hdr->ownerHeap, hdr);
    else
        sym_User_Free(hdr);
}

extern "C" void* calloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > (0x70000000u / nmemb))
        return 0;
    size_t total = nmemb * size;
    void* p = malloc(total ? total : 1u);
    if (p)
        memset(p, 0, total ? total : 1u);
    return p;
}

extern "C" void* realloc(void* ptr, size_t size)
{
    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return 0;
    }
    PfAllocHeader* oldHdr = ((PfAllocHeader*)ptr) - 1;
    if (size <= oldHdr->size)
        return ptr;

    void* nw = malloc(size);
    if (!nw)
        return 0;
    memcpy(nw, ptr, oldHdr->size);
    free(ptr);
    return nw;
}

extern "C" void abort(void)
{
    sym_User_Invariant();
}

extern "C" void qsort(void* base, size_t nmemb, size_t size,
                      int (*compar)(const void*, const void*))
{
    if (!base || nmemb < 2u || size == 0u || !compar)
        return;
    uint8_t* arr = (uint8_t*)base;
    /* Shellsort (Ciura gaps): O(N^1.25), zero recursion, zero heap, zero WSD */
    static const size_t kGaps[] = { 701u, 301u, 132u, 57u, 23u, 10u, 4u, 1u };
    for (size_t gi = 0; gi < sizeof(kGaps) / sizeof(kGaps[0]); ++gi) {
        size_t gap = kGaps[gi];
        if (gap >= nmemb)
            continue;
        for (size_t i = gap; i < nmemb; ++i) {
            size_t j = i;
            while (j >= gap) {
                uint8_t* a = arr + (j - gap) * size;
                uint8_t* b = arr + j * size;
                if (compar(a, b) <= 0)
                    break;
                for (size_t k = 0; k < size; ++k) {
                    uint8_t t = a[k];
                    a[k] = b[k];
                    b[k] = t;
                }
                j -= gap;
            }
        }
    }
}

/* --- C++ freestanding operators & ABI stubs ------------------------------- */

namespace std {
    const nothrow_t nothrow = nothrow_t();
}

void* operator new(size_t size) { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void* operator new(size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void  operator delete(void* ptr) noexcept { free(ptr); }
void  operator delete[](void* ptr) noexcept { free(ptr); }
void  operator delete(void* ptr, size_t) noexcept { free(ptr); }
void  operator delete[](void* ptr, size_t) noexcept { free(ptr); }

extern "C" void __cxa_pure_virtual(void)
{
    sym_User_Invariant();
}

/* --- String & memory primitives backed by euser ROM ----------------------- */

extern "C" void* memcpy(void* dst, const void* src, size_t n)
{
    if (n == 0u || dst == src)
        return dst;
    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)n) & 3u) == 0u)
        return sym_wordmove(dst, src, (unsigned int)n);
    return sym_memmove(dst, src, (unsigned int)n);
}

extern "C" void* memmove(void* dst, const void* src, size_t n)
{
    if (n == 0u || dst == src)
        return dst;
    if ((((uintptr_t)dst | (uintptr_t)src | (uintptr_t)n) & 3u) == 0u &&
        (uintptr_t)dst < (uintptr_t)src) {
        return sym_wordmove(dst, src, (unsigned int)n);
    }
    return sym_memmove(dst, src, (unsigned int)n);
}

extern "C" void* memset(void* s, int c, size_t n)
{
    unsigned char byte = (unsigned char)c;
    if (n == 0u)
        return s;
    if (byte == 0u) {
        sym_memclr(s, (unsigned int)n);
        return s;
    }

    unsigned char* p = (unsigned char*)s;
    while (n != 0u && ((uintptr_t)p & 3u) != 0u) {
        *p++ = byte;
        --n;
    }
    uint32_t w = (uint32_t)byte * 0x01010101u;
    while (n >= 16u) {
        uint32_t* wp = (uint32_t*)p;
        wp[0] = w; wp[1] = w; wp[2] = w; wp[3] = w;
        p += 16;
        n -= 16u;
    }
    while (n >= 4u) {
        *(uint32_t*)p = w;
        p += 4;
        n -= 4u;
    }
    while (n-- != 0u)
        *p++ = byte;
    return s;
}

extern "C" int memcmp(const void* s1, const void* s2, size_t n)
{
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    while (n-- != 0u) {
        int diff = (int)*a++ - (int)*b++;
        if (diff != 0)
            return diff;
    }
    return 0;
}

extern "C" size_t strlen(const char* s)
{
    const char* p = s;
    while (*p != '\0')
        ++p;
    return (size_t)(p - s);
}

extern "C" int strcmp(const char* s1, const char* s2)
{
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    for (;;) {
        unsigned char ca = *a++;
        unsigned char cb = *b++;
        if (ca != cb || ca == 0u)
            return (int)ca - (int)cb;
    }
}

extern "C" int strncmp(const char* s1, const char* s2, size_t n)
{
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    while (n-- != 0u) {
        unsigned char ca = *a++;
        unsigned char cb = *b++;
        if (ca != cb || ca == 0u)
            return (int)ca - (int)cb;
    }
    return 0;
}

extern "C" char* strcpy(char* dst, const char* src)
{
    char* d = dst;
    while ((*d++ = *src++) != '\0') {}
    return dst;
}

extern "C" char* strncpy(char* dst, const char* src, size_t n)
{
    char* d = dst;
    while (n != 0u && *src != '\0') {
        *d++ = *src++;
        --n;
    }
    while (n-- != 0u)
        *d++ = '\0';
    return dst;
}

extern "C" const char* strstr(const char* haystack, const char* needle)
{
    if (!*needle)
        return haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; ++haystack) {
        if (*haystack == *needle && strncmp(haystack, needle, nlen) == 0)
            return haystack;
    }
    return 0;
}

/* Minimal sprintf/snprintf supporting %s, %d, %u, %x, %X, %08x, %08X */
static int pf_vsnprintf(char* buf, size_t cap, const char* fmt, va_list ap)
{
    size_t pos = 0;
    auto putc_out = [&](char ch) {
        if (buf && pos + 1 < cap)
            buf[pos] = ch;
        ++pos;
    };

    while (fmt && *fmt) {
        if (*fmt != '%') {
            putc_out(*fmt++);
            continue;
        }
        ++fmt;
        if (*fmt == '%') {
            putc_out(*fmt++);
            continue;
        }
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; ++fmt; }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            ++fmt;
        }
        if (*fmt == 'l') ++fmt;
        if (*fmt == 'l') ++fmt;
        if (*fmt == 'z') ++fmt;

        char spec = *fmt ? *fmt++ : '\0';
        if (spec == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s) putc_out(*s++);
        } else if (spec == 'd' || spec == 'i' || spec == 'u' ||
                   spec == 'x' || spec == 'X' || spec == 'p') {
            uint32_t uval;
            bool neg = false;
            int base = (spec == 'x' || spec == 'X' || spec == 'p') ? 16 : 10;
            bool upper = (spec == 'X');
            if (spec == 'd' || spec == 'i') {
                int iv = va_arg(ap, int);
                if (iv < 0) { neg = true; uval = (uint32_t)(-(int64_t)iv); }
                else { uval = (uint32_t)iv; }
            } else {
                uval = va_arg(ap, uint32_t);
            }
            char tmp[16];
            int len = 0;
            do {
                uint32_t d = uval % (uint32_t)base;
                tmp[len++] = (char)(d < 10 ? ('0' + d) : ((upper ? 'A' : 'a') + (d - 10)));
                uval /= (uint32_t)base;
            } while (uval != 0);
            if (neg && pad == '0') putc_out('-');
            int totalDigits = len + (neg ? 1 : 0);
            while (totalDigits < width) { putc_out(pad); ++totalDigits; }
            if (neg && pad != '0') putc_out('-');
            while (len > 0) putc_out(tmp[--len]);
        }
    }
    if (buf && cap > 0)
        buf[(pos < cap) ? pos : (cap - 1)] = '\0';
    return (int)pos;
}

extern "C" int snprintf(char* buf, size_t size, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = pf_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

extern "C" int sprintf(char* buf, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = pf_vsnprintf(buf, 0x7FFFFFFFu, fmt, ap);
    va_end(ap);
    return r;
}

/* --- IEEE-754 single-precision math (floorf, ceilf, sqrtf) ---------------- */

static inline uint32_t pf_f2u(float x)
{
    uint32_t u;
    memcpy(&u, &x, sizeof(u));
    return u;
}

static inline float pf_u2f(uint32_t u)
{
    float x;
    memcpy(&x, &u, sizeof(x));
    return x;
}

extern "C" float floorf(float x)
{
    uint32_t u = pf_f2u(x);
    int e = (int)((u >> 23) & 0xFFu) - 127;
    if (e >= 23)
        return x;
    if (e < 0) {
        if ((u & 0x7FFFFFFFu) == 0u)
            return x;
        return (u >> 31) ? -1.0f : 0.0f;
    }
    uint32_t mask = (1u << (23 - e)) - 1u;
    if ((u & mask) == 0u)
        return x;
    if (u >> 31)
        u += (1u << (23 - e));
    u &= ~mask;
    return pf_u2f(u);
}

extern "C" float ceilf(float x)
{
    return -floorf(-x);
}

extern "C" float sqrtf(float x)
{
    uint32_t u = pf_f2u(x);
    if ((u & 0x7FFFFFFFu) == 0u)
        return x;
    if (u >> 31)
        return pf_u2f(0x7FC00000u);
    uint32_t exp_raw = (u >> 23) & 0xFFu;
    if (exp_raw == 0xFFu)
        return x;

    int e;
    uint32_t m = u & 0x7FFFFFu;
    if (exp_raw == 0u) {
        e = -126;
        while ((m & (1u << 23)) == 0u) {
            m <<= 1;
            --e;
        }
    } else {
        e = (int)exp_raw - 127;
        m |= (1u << 23);
    }

    if (e & 1) {
        m <<= 1;
        --e;
    }

    uint64_t n = (uint64_t)m << 25;
    uint64_t r = 0;
    uint64_t bit = 1ULL << 48;
    while (bit != 0ULL) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }

    uint32_t q = (uint32_t)(r >> 1);
    if ((r & 1ULL) && (n != 0ULL || (q & 1u)))
        ++q;

    uint32_t out_exp = (uint32_t)((e >> 1) + 127);
    if (q >= (1u << 24)) {
        q >>= 1;
        ++out_exp;
    }
    return pf_u2f((out_exp << 23) | (q & 0x7FFFFFu));
}
