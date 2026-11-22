/* hgFloat.c - IEEE 754 bit-pattern float helpers.
 *
 * Clean port of the decompiled hgFloat.c / hgFloat.inl code from the Gerbera
 * GLES1 driver.  The F_* operations never touch floating point hardware:
 * everything happens on the raw bit patterns, which makes compares
 * NaN-safe and keeps the code identical across FPUs.
 */
#include "hg/hgFloat.h"
#include "hg/hgInt32.h"
#include "hg/hgDefs.h"

/* ------------------------------------------------------------------ */
/* validity                                                           */
/* ------------------------------------------------------------------ */

HGint32 F_ISVALID(HGuint32 a)
{
    /* NaN has exponent 255 with nonzero mantissa, Inf has mantissa 0:
     * anything with exponent field <= 255 and not (255, nonzero) passes.
     * The original only rejects NaN (mantissa nonzero, exp 255). */
    return (a & 0x7FFFFFFFu) <= 0x7F800000u;
}

HGint32 F_FINITE(HGuint32 a)
{
    return (a & 0x7F800000u) != 0x7F800000u;
}

/* ------------------------------------------------------------------ */
/* total order compares                                               */
/* ------------------------------------------------------------------ */

HGint32 F_MAKE_COMPARABLE(HGuint32 a)
{
    /* map sign-magnitude ordering onto two's complement ordering */
    if (a & 0x80000000u)
        return (HGint32)(0x80000000u - a);
    return (HGint32)a;
}

HGint32 F_CMPLE(HGuint32 a, HGuint32 b)
{
    HGint32 x = F_MAKE_COMPARABLE(a);
    return x <= F_MAKE_COMPARABLE(b);
}

HGint32 F_CMPGE(HGuint32 a, HGuint32 b)
{
    HGint32 x = F_MAKE_COMPARABLE(a);
    return x >= F_MAKE_COMPARABLE(b);
}

HGint32 F_CMPLT(HGuint32 a, HGuint32 b)
{
    HGint32 x = F_MAKE_COMPARABLE(a);
    return x < F_MAKE_COMPARABLE(b);
}

HGint32 F_CMPGT(HGuint32 a, HGuint32 b)
{
    HGint32 x = F_MAKE_COMPARABLE(a);
    return x > F_MAKE_COMPARABLE(b);
}

HGint32 F_CMPEQ(HGuint32 a, HGuint32 b)
{
    HGuint32 aa = b ^ a;
    if (aa == 0x80000000u)       /* +0.0 == -0.0 */
        aa = b & 0x7FFFFFFFu;
    return aa == 0;
}

HGint32 F_CMPNE(HGuint32 a, HGuint32 b)
{
    HGuint32 aa = b ^ a;
    if (aa == 0x80000000u)
        aa = b & 0x7FFFFFFFu;
    return aa != 0;
}

/* ------------------------------------------------------------------ */
/* zero compares                                                      */
/* ------------------------------------------------------------------ */

HGint32 F_ZCMPEQ(HGuint32 a) { return 2 * a == 0; }
HGint32 F_ZCMPNE(HGuint32 a) { return 2 * a != 0; }

HGint32 F_ZCMPLT(HGuint32 a)
{
    return a > 0x80000000u;      /* negative and not -0.0 */
}

HGint32 F_ZCMPLE(HGuint32 a)
{
    return (HGint32)a <= 0;
}

HGint32 F_ZCMPGE(HGuint32 a)
{
    return a <= 0x80000000u;     /* positive and not +Inf */
}

HGint32 F_ZCMPGT(HGuint32 a)
{
    return (HGint32)a > 0;
}

/* ------------------------------------------------------------------ */
/* arithmetic on bit patterns                                         */
/* ------------------------------------------------------------------ */

HGuint32 F_ABS(HGuint32 a)
{
    return a & 0x7FFFFFFFu;
}

HGuint32 F_NEG(HGuint32 a)
{
    return a ^ 0x80000000u;
}

HGuint32 F_MIN(HGuint32 a, HGuint32 b)
{
    /* sign-magnitude comparison via two's complement mapping */
    HGint32 ca = (HGint32)(a ^ ((HGuint32)((HGint32)a >> 30) >> 1));
    HGint32 cb = (HGint32)(b ^ ((HGuint32)((HGint32)b >> 30) >> 1));
    return ca >= cb ? b : a;
}

HGuint32 F_MAX(HGuint32 a, HGuint32 b)
{
    HGint32 ca = (HGint32)(a ^ ((HGuint32)((HGint32)a >> 30) >> 1));
    HGint32 cb = (HGint32)(b ^ ((HGuint32)((HGint32)b >> 30) >> 1));
    return ca >= cb ? a : b;
}

HGuint32 F_MAXZERO(HGuint32 a)
{
    return hgMaxZero32((HGint32)a);
}

/* ------------------------------------------------------------------ */
/* float construction (hgFloat.c)                                     */
/* ------------------------------------------------------------------ */

HGuint32 extractMantissa(HGuint32 a)
{
    return (a & 0x7FFFFFu) | 0x800000u;
}

HGint32 extractBiasedExponent(HGuint32 a)
{
    return (int)((2 * a) >> 24);
}

HGuint32 lsrSigned(HGuint32 x, int shift)
{
    if (shift < 0)
        return x << (-(HGint8)(HGint8)shift);
    return x >> shift;
}

HGuint32 buildFloat(HGuint32 signMask, int exponent, HGuint32 mantissa)
{
    HGuint32 retValue;
    HG_ASSERT_AT("hgFloat.c", 732, signMask == 0 || signMask == 0x80000000u);
    HG_ASSERT_AT("hgFloat.c", 733, mantissa < (1u << 24));
    retValue = (mantissa & 0x7FFFFFu) | ((HGuint32)exponent << 23);
    if (exponent >= 255)
        retValue = 2139095039u;   /* +Inf */
    if (exponent <= 0)
        return 0;                 /* flush to zero */
    HG_ASSERT_AT("hgFloat.c", 749, F_ISVALID(signMask | retValue));
    return signMask | retValue;
}

HGuint32 buildFloatUnchecked(HGuint32 signMask, int exponent, HGuint32 mantissa)
{
    HGuint32 retValue = (mantissa & 0x7FFFFFu) | signMask | ((HGuint32)exponent << 23);
    HG_ASSERT_AT("hgFloat.c", 770, signMask == 0 || signMask == 0x80000000u);
    HG_ASSERT_AT("hgFloat.c", 771, mantissa < (1u << 24));
    HG_ASSERT_AT("hgFloat.c", 772, exponent >= 0 && exponent <= 255);
    HG_ASSERT_AT("hgFloat.c", 784, F_ISVALID(retValue));
    HG_ASSERT_AT("hgFloat.c", 785, F_FINITE(retValue));
    return retValue;
}

/* ------------------------------------------------------------------ */
/* F_ADD - bit pattern float addition (hgFloat.c:812)                 */
/* ------------------------------------------------------------------ */

HGuint32 F_ADD(HGuint32 a, HGuint32 b)
{
    HGuint32 signDiff = (b ^ a) & 0x80000000u;   /* 0: add, else subtract */
    HGuint32 sign     = a & 0x80000000u;
    HGuint32 aa = F_ABS(a);
    HGuint32 ba = F_ABS(b);
    HGuint32 aExp, expDiff, aMan, bMan, carry;

    HG_ASSERT_AT("hgFloat.c", 812, F_ISVALID(a) && F_ISVALID(b));

    if (aa <= ba)
    {
        HGuint32 t = aa;
        aa = ba;
        ba = t;
        sign ^= signDiff;
        if (ba == t && signDiff != 0)
            return 0;             /* equal magnitude, opposite sign */
    }

    if (ba < 0x800000u)           /* b is denormal or zero: result is a */
        return sign ^ aa;

    aExp    = aa >> 23;
    expDiff = (aa >> 23) - (ba >> 23);
    if (expDiff > 0x1Fu)
        expDiff = 31;
    aMan = extractMantissa(aa) << 7;
    bMan = extractMantissa(ba) << 7;

    if (signDiff != 0)
    {
        /* subtract |b| from |a| with sticky rounding */
        HGuint32 bManFull = bMan;
        bMan >>= expDiff;
        if (bManFull != (bMan << expDiff))
            ++bMan;
        aMan -= bMan;
        {
            /* renormalize: clz-1 puts the top bit at position 30 */
            HGint32 sh = hgClz32(aMan) - 1;
            aExp -= sh;
            aMan <<= sh;
        }
    }
    else
    {
        bMan >>= expDiff;
        aMan += bMan;
        carry = aMan >> 31;
        aExp += carry;
        aMan >>= (aMan & 0x80000000u) != 0;
    }
    return buildFloat(sign, (HGint32)aExp, aMan >> 7);
}

HGuint32 F_SUB(HGuint32 a, HGuint32 b)
{
    return F_ADD(a, F_NEG(b));
}

/* ------------------------------------------------------------------ */
/* unit scalar trig (hgFloat.c:2337-2460)                             */
/* ------------------------------------------------------------------ */

HGint32 radiansToUnitScalar(HGuint32 a)
{
    /* convert a float in radians into a 2.30-ish unit scalar */
    HGint32 e = extractBiasedExponent(a);
    HGuint32 m = extractMantissa(a);
    HGint32 mm = (HGint32)(m << 6);
    HGint32 lsh, rsh;
    HGint64 scaled;

    if (a & 0x80000000u)
        mm = -(HGint32)(m * 64);
    lsh = hgMin32(hgMaxZero32(e - 125), 63);
    rsh = hgMin32(hgMaxZero32(125 - e) + 2, 31);
    scaled = hgMul64(mm, 1367130513);   /* 2^33 / pi rounded */
    return hgLsl64h(scaled, lsh) >> rsh;
}

HGuint32 unitScalarToFloat(int s)
{
    int mantissa = hgAbs32(s);
    int exponent = 8 - hgClz32((HGuint32)mantissa);
    HGuint32 m;
    HG_ASSERT_AT("hgFloat.c", 2460, hgAbs32(s) <= 0x20000000);
    m = lsrSigned((HGuint32)mantissa, exponent);
    return buildFloatUnchecked((HGuint32)(s & 0x80000000), exponent + 121, m);
}

HGint32 IUnitScalar_sin(int x)
{
    /* Taylor series of sin on a 2.30 unit scalar; original hgFloat.c:2337 */
    HGint32 y = hgAbs32(x);
    HGint32 x2, ya, yb, yc, yd, ye, yf, r;
    HGint64 s;

    HG_ASSERT_AT("hgFloat.c", 2337, (HGuint32)y <= (HGuint32)0x20000000);
    if (y > 0x10000000)
        y = 0x20000000 - y;
    s  = (HGint64)hgMulu64(4u * (HGuint32)y, 0xC90FDAA2u);
    ya = hgLsl64h_0_31(s, 2);
    x2 = hgMulu64h((HGuint32)ya, (HGuint32)ya);
    yb = (HGint32)hgMulu64h((HGuint32)x2, (HGuint32)ya);
    s  = hgMadd64(s, yb, -715827882);
    yc = (HGint32)hgMulu64h((HGuint32)x2, (HGuint32)yb);
    s  = hgMadd64(s, yc, 143165576);
    yd = (HGint32)hgMulu64h((HGuint32)x2, (HGuint32)yc);
    s  = hgMadd64(s, yd, -13634817);
    ye = (HGint32)hgMulu64h((HGuint32)x2, (HGuint32)yd);
    s  = hgMadd64(s, ye, 757488);
    yf = (HGint32)hgMulu64h((HGuint32)x2, (HGuint32)ye);
    r  = hgMadd64h(s, yf, -27545);
    if (x < 0)
        r = -r;
    HG_ASSERT_AT("hgFloat.c", 2361, r >= -0x20000000);
    HG_ASSERT_AT("hgFloat.c", 2362, r <= 0x20000000);
    return r;
}

HGint32 IUnitScalar_cos(int x)
{
    /* Taylor series of cos on a 2.30 unit scalar; original hgFloat.c:2407 */
    HGint32 xa = hgAbs32(x);
    HGuint32 y = (HGuint32)xa;
    HGint32 x2, t, ta, tb, tc, td, r;
    HGint64 c;

    if (y > 0x10000000u)
        y = 0x20000000u - y;
    x2 = (HGint32)hgMulu64h(8u * y, 0xC90FDAA2u);
    c  = (HGint64)hgMulu64((HGuint32)x2, 2u * (HGuint32)x2);
    x2 = hgLsl64h_0_31(c, 1);
    t  = (HGint32)hgMulu64h((HGuint32)x2, (HGuint32)x2);
    c  = hgMadd64(c, t, -715827883);
    ta = (HGint32)hgMulu64h((HGuint32)t, (HGuint32)x2);
    c  = hgMadd64(c, ta, 95443717);
    tb = (HGint32)hgMulu64h((HGuint32)ta, (HGuint32)x2);
    c  = hgMadd64(c, tb, -6817407);
    tc = (HGint32)hgMulu64h((HGuint32)tb, (HGuint32)x2);
    c  = hgMadd64(c, tc, 302996);
    td = (HGint32)hgMulu64h((HGuint32)tc, (HGuint32)x2);
    c  = hgMadd64(c, td, -9181);
    r  = (0x40000000 - hgGet64h(c)) >> 1;
    if (xa > 0x10000000)
        r = -r;
    HG_ASSERT_AT("hgFloat.c", 2407, r >= -0x20000000);
    HG_ASSERT_AT("hgFloat.c", 2408, r <= 0x20000000);
    return r;
}

HGuint32 F_SIN(HGuint32 a)
{
    /* sin(x) ~= x for tiny angles: pass the radians bit pattern through */
    if (F_ABS(a) <= 0x3A000000u)
        return a;
    return unitScalarToFloat(IUnitScalar_sin(radiansToUnitScalar(a)));
}

HGuint32 F_COS(HGuint32 a)
{
    return unitScalarToFloat(IUnitScalar_cos(radiansToUnitScalar(a)));
}

HGfloat hgFsin(HGfloat a)
{
    return FOUT(F_SIN(FIN(a)));
}

HGfloat hgFcos(HGfloat a)
{
    return FOUT(F_COS(FIN(a)));
}
