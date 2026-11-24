/* hgFill.c - fast fill routines.
 *
 * hgFill32_inner handles the remainder before an eight-word unrolled loop.
 * hgFill8 and hgFill16 align the pointer and replicate the value into a
 * 32-bit pattern; hgFill32 requires word alignment.
 */
#include "hg/hgFill.h"
#include "hg/hgDefs.h"
#ifdef __SYMBIAN32__
#include "symbian_os.h"
#endif

void hgFill32_inner(HGuint32 *d, HGuint32 v, int N)
{
    /* original assert: hgFill.c:114 "d != 0 && N >= 0 && (N == 0 || !((HGuintptr)(d) & 3))" */
    HG_ASSERT_AT("hgFill.c", 114, d != 0 && N >= 0 && (N == 0 || !((HGuintptr)d & 3)));

#ifdef __SYMBIAN32__
    if (v == 0u && N >= 16)
    {
        sym_memclr(d, (unsigned int)N << 2);
        return;
    }
#endif

    switch (N & 7)
    {
    case 0: break;
    case 7: d[6] = v; /* fall through */
    case 6: d[5] = v; /* fall through */
    case 5: d[4] = v; /* fall through */
    case 4: d[3] = v; /* fall through */
    case 3: d[2] = v; /* fall through */
    case 2: d[1] = v; /* fall through */
    case 1: d[0] = v; /* fall through */
    default: break;
    }
    {
        HGuint32 *da = &d[N & 7];
        int Na = N >> 3;
#if defined(__arm__) && !defined(__thumb__)
        if (Na > 0)
        {
            register HGuint32 v0 __asm__("r2") = v;
            register HGuint32 v1 __asm__("r3") = v;
            register HGuint32 v2 __asm__("r4") = v;
            register HGuint32 v3 __asm__("r5") = v;
            do
            {
                __asm__ volatile(
                    "stmia %0!, {%1, %2, %3, %4}\n\t"
                    "stmia %0!, {%1, %2, %3, %4}"
                    : "+r"(da)
                    : "r"(v0), "r"(v1), "r"(v2), "r"(v3)
                    : "memory");
            } while (--Na != 0);
        }
#else
        while (Na-- != 0)
        {
            da[0] = v;
            da[1] = v;
            da[2] = v;
            da[3] = v;
            da[4] = v;
            da[5] = v;
            da[6] = v;
            da[7] = v;
            da += 8;
        }
#endif
    }
}

void hgFill8(HGuint8 *d, HGuint8 v, int N)
{
    HG_ASSERT_AT("hgFill.c", 153, d != 0 && N >= 0);
    while (N != 0 && ((HGuintptr)d & 3) != 0)
    {
        *d++ = v;
        --N;
    }
    while ((N & 3) != 0)
        d[--N] = v;
    hgFill32_inner((HGuint32 *)d, (HGuint32)v * 0x01010101u, N >> 2);
}

void hgFill16(HGuint16 *d, HGuint16 v, int N)
{
    HG_ASSERT_AT("hgFill.c", 189, !((HGuintptr)d & 1) && d != 0 && N >= 0);
    if (N > 0)
    {
        d[N - 1] = v;   /* cover an unpaired halfword before the word fill */
        if (((HGuintptr)d & 3) != 0)
        {
            *d++ = v;
            --N;
        }
        hgFill32_inner((HGuint32 *)d, (HGuint32)v * 0x00010001u, N >> 1);
    }
}

void hgFill32(HGuint32 *d, HGuint32 v, int N)
{
    HG_ASSERT_AT("hgFill.c", 222, !((HGuintptr)d & 3) && d != 0 && N >= 0);
    hgFill32_inner(d, v, N);
}
