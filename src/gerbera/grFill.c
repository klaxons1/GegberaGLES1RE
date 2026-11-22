/* grFill.c - pattern/masked fills (port of the decompiled grFill.inl).
 *
 * The dithered color clear walks rows through grFill4x8/16/32 with four
 * per-pixel dithered colors; the plain clears go through grFillNMasked with
 * a single value (the NMasked variants are also the off_479274 fill table).
 * Assert sites keep the original grFill.inl file/line strings.
 */
#include "gerbera/grUtil.h"
#include "gerbera/grDefs.h"
#include "hg/hgInt32.h"
#include "hg/hgFill.h"

#define GR_FILL_CHECK(cond, file, line)          \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, (file), (line)) != 0) \
            grDie(0);                            \
    } while (0)

/* --- grFill8Masked (grFill.inl 30) ---------------------------------------- */

void grFill8Masked(unsigned char *dst, unsigned int value, unsigned int mask, int size)
{
    unsigned char valuea;

    GR_FILL_CHECK(dst != NULL, "grFill.inl", 37);
    GR_FILL_CHECK(size >= 0, "grFill.inl", 38);
    if ((unsigned char)mask == 255)
    {
        hgFill8(dst, (unsigned char)value, size);
    }
    else
    {
        valuea = (unsigned char)(mask & value);
        while (size-- != 0)
        {
            *dst = (unsigned char)(valuea | (~mask & *dst));
            ++dst;
        }
    }
}

/* --- grFill16Masked (grFill.inl 61) --------------------------------------- */

void grFill16Masked(unsigned short *dst, unsigned int value, unsigned int mask, int size)
{
    unsigned short valuea;

    GR_FILL_CHECK(dst != NULL, "grFill.inl", 68);
    GR_FILL_CHECK(size >= 0, "grFill.inl", 69);
    GR_FILL_CHECK(!((unsigned int)(uintptr_t)dst & 1), "grFill.inl", 70);
    if ((unsigned short)mask == 0xFFFF)
    {
        hgFill16(dst, (unsigned short)value, size);
    }
    else
    {
        valuea = (unsigned short)(mask & value);
        while (size-- != 0)
        {
            *dst = (unsigned short)(valuea | (~mask & *dst));
            ++dst;
        }
    }
}

/* --- grFill32Masked (grFill.inl 93) --------------------------------------- */

void grFill32Masked(unsigned int *dst, unsigned int value, unsigned int mask, int size)
{
    unsigned int valuea;

    GR_FILL_CHECK(dst != NULL, "grFill.inl", 100);
    GR_FILL_CHECK(size >= 0, "grFill.inl", 101);
    GR_FILL_CHECK(!((unsigned int)(uintptr_t)dst & 3), "grFill.inl", 102);
    if (mask == 0xFFFFFFFFu)
    {
        hgFill32(dst, value, size);
    }
    else
    {
        valuea = mask & value;
        while (size-- != 0)
        {
            *dst = valuea | (~mask & *dst);
            ++dst;
        }
    }
}

/* --- grFill4x8 (grFill.inl 138) ------------------------------------------- */

void grFill4x8(unsigned char *dst, unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned int mask, int size)
{
    unsigned char aa, ba, ca, da, t;
    int maska, fill, n;

    maska = (int)(((((unsigned int)(unsigned char)mask << 8) | (unsigned char)mask) << 16)
          | (((unsigned int)(unsigned char)mask << 8) | (unsigned char)mask));
    aa = (unsigned char)(maska & a);
    ba = (unsigned char)(maska & b);
    ca = (unsigned char)(maska & c);
    da = (unsigned char)(maska & d);
    GR_FILL_CHECK(dst != NULL, "grFill.inl", 145);
    GR_FILL_CHECK(size > 0, "grFill.inl", 146);
    if (size >= 4)
    {
        n = (int)((4 - (unsigned int)(uintptr_t)dst) & 3);
        while (n-- != 0)
        {
            *dst = (unsigned char)(aa + (*dst & ~(unsigned char)maska));
            ++dst;
            t = aa; aa = ba; ba = ca; ca = da; da = t;
            --size;
        }
        GR_FILL_CHECK(grIsAligned(dst, 4), "grFill.inl", 160);
        if (hgGetEndianess() == HG_TRUE) /* big endian (this hg returns TRUE for BIG; decompile: != HG_ENDIAN_LITTLE) */
            fill = (int)((unsigned int)da | ((unsigned int)ca << 8) | ((unsigned int)aa << 24) | ((unsigned int)ba << 16));
        else
            fill = (int)((unsigned int)aa | ((unsigned int)ba << 8) | ((unsigned int)da << 24) | ((unsigned int)ca << 16));
        if (maska == (int)0xFFFFFFFFu)
            hgFill32((unsigned int *)dst, (unsigned int)fill, size >> 2);
        else
            grFill32Masked((unsigned int *)dst, (unsigned int)fill, (unsigned int)maska, size >> 2);
        dst += size & 0xFFFFFFFC;
    }
    n = size & 3;
    while (n-- != 0)
    {
        *dst = (unsigned char)(aa + (*dst & ~(unsigned char)maska));
        ++dst;
        aa = ba;
        ba = ca;
    }
}

/* --- grFill4x16 (grFill.inl 199) ------------------------------------------ */

void grFill4x16(unsigned short *dst, unsigned short a, unsigned short b, unsigned short c, unsigned short d, unsigned int mask, int size)
{
    unsigned short aa, ba, ca, da, t;
    unsigned int maska, fill1, fill2;
    unsigned short *d32;
    int n;

    maska = (mask << 16) | (unsigned short)mask;
    aa = (unsigned short)(maska & a);
    ba = (unsigned short)(maska & b);
    ca = (unsigned short)(maska & c);
    da = (unsigned short)(maska & d);
    GR_FILL_CHECK(dst != NULL && grIsAligned(dst, 2), "grFill.inl", 206);
    GR_FILL_CHECK(size > 0, "grFill.inl", 207);
    if (grIsAligned(dst, 4) == 0)
    {
        *dst = (unsigned short)(aa + (*dst & ~(unsigned short)maska));
        ++dst;
        t = aa; aa = ba; ba = ca; ca = da; da = t;
        --size;
    }
    GR_FILL_CHECK(grIsAligned(dst, 4), "grFill.inl", 218);
    n = size >> 2;
    if (n != 0)
    {
        d32 = dst;
        if (hgGetEndianess() == HG_TRUE) /* big endian (this hg returns TRUE for BIG; decompile: != HG_ENDIAN_LITTLE) */
        {
            fill1 = (unsigned int)ba | ((unsigned int)aa << 16);
            fill2 = (unsigned int)da | ((unsigned int)ca << 16);
        }
        else
        {
            fill1 = (unsigned int)aa | ((unsigned int)ba << 16);
            fill2 = (unsigned int)ca | ((unsigned int)da << 16);
        }
        GR_FILL_CHECK((~maska & fill1) == 0, "grFill.inl", 238);
        GR_FILL_CHECK((~maska & fill2) == 0, "grFill.inl", 239);
        if (maska == 0xFFFFFFFFu)
        {
            do
            {
                *(unsigned int *)d32 = fill1;
                *(unsigned int *)(d32 + 2) = fill2;
                d32 += 4;
                --n;
            } while (n != 0);
        }
        else
        {
            do
            {
                *(unsigned int *)d32 = fill1 + (*(unsigned int *)d32 & ~maska);
                *((unsigned int *)d32 + 1) = fill2 + (*((unsigned int *)d32 + 1) & ~maska);
                d32 += 4;
                --n;
            } while (n != 0);
        }
        dst = d32;
    }
    n = size & 3;
    while (n-- != 0)
    {
        *dst = (unsigned short)(aa + (*dst & ~(unsigned short)maska));
        ++dst;
        aa = ba;
        ba = ca;
    }
}

/* --- grFill4x32 (grFill.inl 280) ------------------------------------------ */

void grFill4x32(unsigned int *dst, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int mask, int size)
{
    unsigned int aa, ba, ca, da;
    int n;

    GR_FILL_CHECK(dst != NULL && grIsAligned(dst, 4), "grFill.inl", 287);
    GR_FILL_CHECK(size > 0, "grFill.inl", 288);
    aa = mask & a;
    ba = mask & b;
    ca = mask & c;
    da = mask & d;
    n = size >> 2;
    if (n != 0)
    {
        if (mask == 0xFFFFFFFFu)
        {
            do
            {
                dst[0] = aa;
                dst[1] = ba;
                dst[2] = ca;
                dst[3] = da;
                dst += 4;
                --n;
            } while (n != 0);
        }
        else
        {
            do
            {
                dst[0] = aa + (dst[0] & ~mask);
                dst[1] = ba + (dst[1] & ~mask);
                dst[2] = ca + (dst[2] & ~mask);
                dst[3] = da + (dst[3] & ~mask);
                dst += 4;
                --n;
            } while (n != 0);
        }
    }
    n = size & 3;
    while (n-- != 0)
    {
        *dst = aa + (*dst & ~mask);
        ++dst;
        aa = ba;
        ba = ca;
    }
}
