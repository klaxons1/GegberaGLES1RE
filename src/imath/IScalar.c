/* IScalar.c - float-as-bits scalar math.
 *
 * Clean port of the decompiled IScalar.c from the Gerbera GLES1 driver.
 * An IScalar is an IEEE single precision float stored in an HGint32; all
 * arithmetic goes through the hg F_* bit-pattern operations.
 */
#include "imath/imApi.h"
#include "hg/hgInt32.h"

/* --- construction / conversion -------------------------------------- */

HGint32 IScalar_clampUnit(HGint32 a)
{
    /* float clamp to [0.0, 1.0] via bit patterns */
    return (HGint32)F_MIN(F_MAXZERO((HGuint32)a), 0x3F800000u);
}

HGint32 IScalar_intToScalar(HGint32 a)
{
    return (HGint32)F_ITOF(a);
}

HGint32 IScalar_iftos(HGint32 a)
{
    /* flush denormals to zero, clamp Inf/NaN into the valid domain */
    HGint32 e = (HGint32)(HGuint8)((HGuint32)a >> 23);
    if ((HGuint8)((HGuint32)a >> 23) == 0)
        a = 0;
    if (e == 255)
        return (HGint32)(((HGuint32)a & 0xFF000000u) | 0x7FFFFFu);
    return a;
}

HGint32 IScalar_ftos(HGfloat a)
{
    return IScalar_iftos((HGint32)hgFloatBitPattern(a));
}

HGfloat IScalar_stof(HGint32 a)
{
    return hgBitPatternToFloat((HGuint32)a);
}

HGint32 IScalar_immediate(HGfloat a)
{
    return (HGint32)hgFloatBitPattern(a);
}

HGint32 IScalar_one(void)
{
    return (HGint32)0x3F800000u;    /* 1.0f */
}

HGint32 IScalar_zero(void)
{
    return 0;
}

HGint32 IScalar_set(HGuint32 e, HGint32 m)
{
    HGuint32 s = (HGuint32)m & 0x80000000u;
    if (e >= 255)
        imAssert("e < 255", "imApi.inl", 168);
    if (!(m > -0x20000000 * 2 && m < 0x20000000 * 2))
        imAssert("m >-0x20000000*2 && m < 0x20000000*2", "imApi.inl", 169);
    if (!(e != 0 || m == 0))
        imAssert("e != 0 || m == 0", "imApi.inl", 171);
    if (s != 0)
        m = -m;
    if (IScalar_isValid((HGint32)(s | (((HGuint32)m >> 6) & 0xFC7FFFFFu) | (e << 23))) == 0)
        imAssert("IScalar_isValid(r)", "imApi.inl", 181);
    return (HGint32)(s | (((HGuint32)m >> 6) & 0xFC7FFFFFu) | (e << 23));
}

HGint32 IScalar_setAndNormalize(HGuint32 e, HGint32 m)
{
    HGint32 sh = hgClz32((HGuint32)hgAbs32(m)) - 1;
    HGint32 ma = (sh >= 0) ? (HGint32)((HGuint32)m << sh) : (m >> 1);
    HGint32 mb = ma >> 1;
    HGint32 ea = (HGint32)e - sh + 1;
    if (ea > 0)
    {
        if ((HGuint32)ea > 0xFE)
        {
            ea = 254;
            mb = 0x3FFFFFFF;
        }
    }
    else
    {
        ea = 0;
        mb = 0;
    }
    return IScalar_set((HGuint32)ea, mb);
}

HGuint32 IScalar_getExponent(HGint32 x)
{
    return (HGuint32)(HGuint8)((HGuint32)x >> 23);
}

HGint32 IScalar_getMantissa(HGint32 x)
{
    HGint32 r = (HGint32)((HGuint32)x & 0x7FFFFFu);
    if ((x & (HGint32)0x7F800000u) != 0)
        r |= 0x800000;
    r <<= 6;                       /* 6.26 fixed point */
    return (x < 0) ? -r : r;
}

HGint32 IScalar_getOESMantissa(HGint32 s)
{
    return IScalar_getMantissa(s);
}

HGint32 IScalar_getOESExponent(HGint32 s)
{
    return (HGint32)IScalar_getExponent(s) - 140;
}

HGuint32 IScalar_getUnsignedMantissa(HGint32 x)
{
    HGuint32 r = (HGuint32)x & 0x7FFFFFu;
    if ((x & (HGint32)0x7F800000u) != 0)
        r |= 0x800000u;
    return r << 6;
}

HGint32 IScalar_toUnitScalar(HGint32 x)
{
    HGint32 r = IScalar_getMantissa(x);
    HGuint32 exponent = IScalar_getExponent(x);
    if ((HGint32)(127 - exponent) <= 0)
    {
        r = (r < 0) ? -536870912 : 0x20000000;
        return r;
    }
    return r >> hgClamp32_0_x(127 - (HGint32)exponent, 31);
}

HGint32 IScalar_fixedToScalar(HGint32 m, HGint32 n)
{
    HGint32 sh = hgClz32((HGuint32)hgAbs32(m)) - 2;
    HGint32 e = 156 - (sh + n);
    HGint32 ma = (sh >= 0) ? (HGint32)((HGuint32)m << sh) : (m >> (-(HGint8)(HGint8)sh));
    if (ma == -1073741824)
    {
        ma = -536870912;
        ++e;
    }
    if (ma == 0)
        e = 0;
    return IScalar_set((HGuint32)e, ma);
}

HGint32 IScalar_scalarToFixed(HGint32 r, HGint32 n)
{
    HGuint32 e = IScalar_getExponent(r);
    HGint32 m = IScalar_getMantissa(r);
    HGint32 sh = 157 - (HGint32)e - n;
    HGint32 ma;
    if (!(n >= 0 && n <= 32))
        imAssert("n >= 0 && n <= 32", "IScalar.c", 1117);
    ma = 2 * m;
    if (sh < 0)
        ma = (ma >> 31) ^ 0x7FFFFFFF;   /* saturate */
    return ma >> hgClamp32_0_x(sh, 31);
}

HGint32 IScalar_byteToScalar(HGuint8 b)
{
    /* b/255 as a scalar, built from a replicated byte pattern */
    HGuint32 pattern = (HGuint32)b | ((HGuint32)b << 8) | ((HGuint32)b << 16) | ((HGuint32)b << 24);
    HGint32 sh = hgClz32(pattern);
    HGuint32 e = (HGuint32)(126 - sh);
    /* SHL masks the count when the zero byte gives clz(0) == 32. */
    HGuint32 m = pattern << (sh & 31);
    HGuint32 r = m + 256;
    if (m + 256 < m)          /* overflow: renormalize */
    {
        ++e;
        r = (r >> 1) | 0x80000000u;
    }
    r >>= 8;
    if (b != 0)
        return (HGint32)((r & 0x7FFFFFu) | (e << 23));
    return 0;
}

HGint32 IScalar_toFixedHO(HGint32 value, HGint32 bits)
{
    if (hgInRange32(bits, 0, 31) == 0)
        imAssert("hgInRange32(bits, 0, 31)", "IScalar.c", 1205);
    return IScalar_scalarToFixed(IScalar_mul(value, IScalar_intToScalar((int)((1u << bits) - 1u))), 0);
}

HGint32 IScalar_toXScalar(HGint32 v)
{
    return IScalar_scalarToFixed(v, 21);
}

/* --- arithmetic ------------------------------------------------------ */

HGint32 IScalar_mul(HGint32 a, HGint32 b)  { return (HGint32)F_MUL((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_add(HGint32 a, HGint32 b)  { return (HGint32)F_ADD((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_sub(HGint32 a, HGint32 b)  { return (HGint32)F_SUB((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_sqr(HGint32 a)             { return (HGint32)F_SQR((HGuint32)a); }
HGint32 IScalar_rcp(HGint32 a)             { return (HGint32)F_RCP((HGuint32)a); }
HGint32 IScalar_rsq(HGint32 a)             { return (HGint32)F_RSQ((HGuint32)a); }
HGint32 IScalar_negate(HGint32 x)          { return (HGint32)F_NEG((HGuint32)x); }
HGint32 IScalar_abs(HGint32 x)             { return (HGint32)F_ABS((HGuint32)x); }
HGint32 IScalar_max(HGint32 a, HGint32 b)  { return (HGint32)F_MAX((HGuint32)a, (HGuint32)b); }

HGint32 IScalar_degreesToRadians(HGint32 x)
{
    /* pi/180 as float bits (0x3C8EFA35, from flt_48E064 in the binary) */
    return (HGint32)F_MUL((HGuint32)x, 0x3C8EFA35u);
}

HGint32 IScalar_cos(HGint32 x)
{
    return (HGint32)F_COS((HGuint32)x);
}

/* --- compares -------------------------------------------------------- */

HGint32 IScalar_cmpeq(HGint32 a, HGint32 b) { return (HGint32)F_CMPEQ((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_cmpne(HGint32 a, HGint32 b) { return (HGint32)F_CMPNE((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_cmplt(HGint32 a, HGint32 b) { return (HGint32)F_CMPLT((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_cmpgt(HGint32 a, HGint32 b) { return (HGint32)F_CMPGT((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_cmple(HGint32 a, HGint32 b) { return (HGint32)F_CMPLE((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_cmpge(HGint32 a, HGint32 b) { return (HGint32)F_CMPGE((HGuint32)a, (HGuint32)b); }
HGint32 IScalar_testeq(HGint32 a)           { return (HGint32)F_ZCMPEQ((HGuint32)a); }
HGint32 IScalar_testne(HGint32 a)           { return (HGint32)F_ZCMPNE((HGuint32)a); }
HGint32 IScalar_testlt(HGint32 a)           { return (HGint32)F_ZCMPLT((HGuint32)a); }
HGint32 IScalar_testgt(HGint32 a)           { return (HGint32)F_ZCMPGT((HGuint32)a); }
HGint32 IScalar_testle(HGint32 a)           { return (HGint32)F_ZCMPLE((HGuint32)a); }
HGint32 IScalar_testge(HGint32 a)           { return (HGint32)F_ZCMPGE((HGuint32)a); }

/* --- log2 ------------------------------------------------------------ */

HGuint32 IScalar_log2_iterate(HGuint32 y)
{
    /* bit-by-bit log2 refinement: result = 0x7FFFFFFF - sum(k * 2^31-ish)
     * over accepted shifts; output is log2 * 2^31 / ... (11.21 fixed). */
    HGuint32 acc = 0x7FFFFFFFu;
    HGuint32 ny;

    ny = y + (y >> 1);
    if (ny >= y) { acc -= 1256197404u; y = ny; }
    ny = y + (y >> 2);
    if (ny >= y) { acc -= 691335319u; y = ny; }
    ny = y + (y >> 3);
    if (ny >= y) { acc -= 364911161u; y = ny; }
    ny = y + (y >> 4);
    if (ny >= y) { acc -= 187825021u; y = ny; }
    ny = y + (y >> 5);
    if (ny >= y) { acc -= 95335645u; y = ny; }
    ny = y + (y >> 6);
    if (ny >= y) { acc -= 48034512u; y = ny; }
    ny = y + (y >> 7);
    if (ny >= y) { acc -= 24110347u; y = ny; }
    ny = y + (y >> 8);
    if (ny >= y) { acc -= 12078627u; y = ny; }
    ny = y + (y >> 9);
    if (ny >= y) { acc -= 6045199u; y = ny; }
    ny = y + (y >> 10);
    if (ny >= y) { acc -= 3024074u; y = ny; }
    ny = y + (y >> 11);
    if (ny >= y) { acc -= 1512406u; y = ny; }
    ny = y + (y >> 12);
    if (ny >= y) { acc -= 756295u; y = ny; }
    ny = y + (y >> 13);
    if (ny >= y) { acc -= 378170u; y = ny; }
    ny = y + (y >> 14);
    if (ny >= y) { acc -= 189091u; y = ny; }
    ny = y + (y >> 15);
    if (ny >= y) { acc -= 94547u; y = ny; }
    ny = y + ((y >> 16) & 0xFFFFu);          /* HIWORD(y) */
    if (ny >= y) { acc -= 47273u; y = ny; }
    ny = y + (y >> 17);
    if (ny >= y) { acc -= 23637u; }
    return acc;
}

HGint32 XScalar_log2(HGint32 y)
{
    HGint32 sh;
    if (!((y >= 0 && y <= 0x7fffffff) || y == (HGint32)0x80000000u))
        imAssert("(y >= 0 && y <= 0x7fffffff) || y == 0x80000000", "IScalar.c", 905);
    if (y <= 0)
        return (HGint32)0x80000000u;
    sh = hgClz32((HGuint32)y);
    return (HGint32)((IScalar_log2_iterate((HGuint32)y << sh) >> 10) - ((HGuint32)(sh - 10) << 21));
}

/* ------------------------------------------------------------------ */
/* IScalar_expe + light attenuation (imApi.inl; need _IFVector3)       */
/* ------------------------------------------------------------------ */

HGint32 IScalar_expe(HGint32 x)
{
    int f;
    if (IScalar_isValid(x) == 0)
        imAssert("IScalar_isValid(x)", "imApi.inl", 0x51u);
    f = imMulAsr32Sat(IScalar_scalarToFixed(x, 21), 774541002, 29);
    return XScalar_exp2(f);
}

HGint32 IScalar_attenuateRcp(HGint32 d, _IFVector3 *ka)
{
    unsigned int m0 = ka->m[0], m1 = ka->m[1], m2 = ka->m[2];
    unsigned int e0 = (HGuint8)ka->e;
    unsigned int e1 = (HGuint8)(ka->e >> 8);
    unsigned int e2 = (HGuint8)(ka->e >> 16);
    unsigned int em;
    int m, e, sh;
    HGint32 r = 0;

    m = 8 * (int)hgMulu64h(IScalar_getMantissa(d), m2);
    e = (int)(e2 + IScalar_getExponent(d)) - 127;
    if (m == 0)
        e = e1;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    sh = hgMin32(hgDiff32(e, e1), 31);
    if (e < (int)e1)
    {
        m = (unsigned int)m >> sh;
        e = e1;
    }
    else
    {
        m1 >>= sh;
    }
    m += (int)m1;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    em = m;
    m = 8 * (int)hgMulu64h(IScalar_getMantissa(d), em);
    e = (int)e + IScalar_getExponent(d) - 127;
    if (m == 0)
        e = 0;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    sh = hgMin32(hgDiff32(e, e0), 31);
    if (e < (int)e0)
    {
        m = (unsigned int)m >> sh;
        e = e0;
    }
    else
    {
        m0 >>= sh;
    }
    if (e < 116)
        return 0x7FFFFFFF;
    m += (int)m0;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    if (m == 0)
        imAssert("m", "imApi.inl", 0x390u);
    if (e <= 148)
    {
        HGint32 v = IScalar_set(e, m);
        r = IScalar_scalarToFixed(IScalar_rcp(v), 21);
    }
    return r;
}

HGint32 IScalar_attenuateRsq(HGint32 d, _IFVector3 *ka)
{
    unsigned int m0 = ka->m[0], m1 = ka->m[1], m2 = ka->m[2];
    unsigned int e0 = (HGuint8)ka->e;
    unsigned int e1 = (HGuint8)(ka->e >> 8);
    unsigned int e2 = (HGuint8)(ka->e >> 16);
    int m, e, sh;

    m = 8 * (int)hgMulu64h(IScalar_getMantissa(d), m2);
    e = (int)(e2 + IScalar_getExponent(d)) - 127;
    if (m == 0)
        e = e1;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    sh = hgMin32(hgDiff32(e, e1), 31);
    if (e < (int)e1)
    {
        m = (unsigned int)m >> sh;
        e = e1;
    }
    else
    {
        m1 >>= sh;
    }
    m += (int)m1;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    m = 8 * (int)hgMulu64h(m, IScalar_getMantissa(d));
    e = (int)e + IScalar_getExponent(d) - 127;
    if (m == 0)
        e = 0;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    sh = hgMin32(hgDiff32(e, e0), 31);
    if (e < (int)e0)
    {
        m = (unsigned int)m >> sh;
        e = e0;
    }
    else
    {
        m0 >>= sh;
    }
    m += (int)m0;
    if ((m & 0x40000000) != 0)
    {
        ++e;
        m = (unsigned int)m >> 1;
    }
    if (e > 0)
    {
        if (e > 254)
        {
            e = 254;
            m = 0x3FFFFFFF;
        }
    }
    else
    {
        e = 0;
        m = 0;
    }
    return IScalar_rsq(IScalar_set(e, m));
}
