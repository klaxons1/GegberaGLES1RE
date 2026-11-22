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
static const HGuint8 s_rcpTable[256] =
{
    0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
    0xF0, 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE5, 0xE4, 0xE3,
    0xE2, 0xE1, 0xE1, 0xE0, 0xDF, 0xDE, 0xDE, 0xDD, 0xDC, 0xDB, 0xDB, 0xDA, 0xD9, 0xD9, 0xD8, 0xD7,
    0xD6, 0xD6, 0xD5, 0xD4, 0xD4, 0xD3, 0xD2, 0xD2, 0xD1, 0xD0, 0xD0, 0xCF, 0xCE, 0xCE, 0xCD, 0xCC,
    0xCC, 0xCB, 0xCA, 0xCA, 0xC9, 0xC9, 0xC8, 0xC7, 0xC7, 0xC6, 0xC5, 0xC5, 0xC4, 0xC4, 0xC3, 0xC3,
    0xC2, 0xC1, 0xC1, 0xC0, 0xC0, 0xBF, 0xBF, 0xBE, 0xBD, 0xBD, 0xBC, 0xBC, 0xBB, 0xBB, 0xBA, 0xBA,
    0xB9, 0xB9, 0xB8, 0xB8, 0xB7, 0xB7, 0xB6, 0xB6, 0xB5, 0xB5, 0xB4, 0xB4, 0xB3, 0xB3, 0xB2, 0xB2,
    0xB1, 0xB1, 0xB0, 0xB0, 0xAF, 0xAF, 0xAE, 0xAE, 0xAD, 0xAD, 0xAC, 0xAC, 0xAC, 0xAB, 0xAB, 0xAA,
    0xAA, 0xA9, 0xA9, 0xA8, 0xA8, 0xA8, 0xA7, 0xA7, 0xA6, 0xA6, 0xA5, 0xA5, 0xA5, 0xA4, 0xA4, 0xA3,
    0xA3, 0xA3, 0xA2, 0xA2, 0xA1, 0xA1, 0xA1, 0xA0, 0xA0, 0x9F, 0x9F, 0x9F, 0x9E, 0x9E, 0x9D, 0x9D,
    0x9D, 0x9C, 0x9C, 0x9C, 0x9B, 0x9B, 0x9A, 0x9A, 0x9A, 0x99, 0x99, 0x99, 0x98, 0x98, 0x98, 0x97,
    0x97, 0x97, 0x96, 0x96, 0x95, 0x95, 0x95, 0x94, 0x94, 0x94, 0x93, 0x93, 0x93, 0x92, 0x92, 0x92,
    0x91, 0x91, 0x91, 0x90, 0x90, 0x90, 0x90, 0x8F, 0x8F, 0x8F, 0x8E, 0x8E, 0x8E, 0x8D, 0x8D, 0x8D,
    0x8C, 0x8C, 0x8C, 0x8C, 0x8B, 0x8B, 0x8B, 0x8A, 0x8A, 0x8A, 0x89, 0x89, 0x89, 0x89, 0x88, 0x88,
    0x88, 0x87, 0x87, 0x87, 0x87, 0x86, 0x86, 0x86, 0x86, 0x85, 0x85, 0x85, 0x84, 0x84, 0x84, 0x84,
    0x83, 0x83, 0x83, 0x83, 0x82, 0x82, 0x82, 0x82, 0x81, 0x81, 0x81, 0x81, 0x80, 0x80, 0x80, 0x80,
};

static const HGint32 s_rsqlut[384] =
{
    394912441, -331663915, 125342815, 380011094, -324097216, 124382248,
    365885216, -316813926, 123443434, 352483682, -309799403, 122525563,
    339759427, -303039962, 121627867, 327669072, -296522804, 120749618,
    316172584, -290235943, 119890125, 305232978, -284168144, 119048728,
    294816046, -278308867, 118224802, 284890114, -272648216, 117417750,
    275425822, -267176891, 116627005, 266395928, -261886143, 115852023,
    257775128, -256767737, 115092288, 249539900, -251813915, 114347307,
    241668350, -247017362, 113616609, 234140088, -242371176, 112899742,
    226936100, -237868837, 112196275, 220038649, -233504189, 111505798,
    213431164, -229271407, 110827913, 207098159, -225164980, 110162244,
    201025146, -221179689, 109508427, 195198562, -217310590, 108866116,
    189605698, -213552995, 108234976, 184234635, -209902457, 107614688,
    179074193, -206354753, 107004943, 174113869, -202905873, 106405447,
    169343796, -199552005, 105815916, 164754692, -196289526, 105236077,
    160337827, -193114986, 104665667, 156084975, -190025102, 104104432,
    151988390, -187016746, 103552131, 148040766, -184086940, 103008527,
    144235209, -181232840, 102473396, 140565215, -178451737, 101946520,
    137024635, -175741043, 101427688, 133607662, -173098287, 100916698,
    130308803, -170521109, 100413354, 127122859, -168007254, 99917467,
    124044908, -165554564, 99428856, 121070291, -163160976, 98947343,
    118194589, -160824515, 98472759, 115413612, -158543291, 98004939,
    112723387, -156315492, 97543724, 110120141, -154139382, 97088960,
    107600292, -152013299, 96640498, 105160435, -149935647, 96198193,
    102797336, -147904895, 95761907, 100507916, -145919574, 95331503,
    98289250, -143978275, 94906851, 96138549, -142079642, 94487824,
    94053160, -140222374, 94074299, 92030556, -138405220, 93666156,
    90068326, -136626978, 93263279, 88164173, -134886491, 92865557,
    86315906, -133182647, 92472880, 84521433, -131514373, 92085143,
    82778758, -129880640, 91702243, 81085974, -128280455, 91324079,
    79441260, -126712860, 90950556, 77842875, -125176934, 90581580,
    76289154, -123671788, 90217057, 74778506, -122196566, 89856901,
    73309406, -120750441, 89501023, 71880399, -119332617, 89149341,
    69811316, -58630450, 22157688, 67177105, -57292834, 21987882,
    64679979, -56005318, 21821922, 62310900, -54765314, 21659664,
    60061548, -53570403, 21500972, 57924255, -52418321, 21345718,
    55891944, -51306950, 21193780, 53958077, -50234305, 21045040,
    52116606, -49198521, 20899389, 50361932, -48197850, 20756721,
    48688866, -47230647, 20616936, 47092591, -46295366, 20479937,
    45568635, -45390552, 20345634, 44112838, -44514831, 20213939,
    42721332, -43666913, 20084768, 41390511, -42845575, 19958043,
    40117013, -42049667, 19833686, 38897705, -41278099, 19711626,
    37729655, -40529841, 19591792, 36610128, -39803921, 19474117,
    35536561, -39099414, 19358537, 34506556, -38415448, 19244992,
    33517868, -37751192, 19133421, 32568390, -37105862, 19023768,
    31656144, -36478711, 18915980, 30779274, -35869029, 18810003,
    29936036, -35276144, 18705788, 29124790, -34699413, 18603285,
    28343991, -34138229, 18502450, 27592186, -33592009, 18403237,
    26868005, -33060202, 18305603, 26170157, -32542280, 18209507,
    25497423, -32037742, 18114908, 24848654, -31546108, 18021768,
    24222762, -31066920, 17930051, 23618721, -30599743, 17839720,
    23035559, -30144158, 17750740, 22472358, -29699767, 17663079,
    21928249, -29266188, 17576704, 21402406, -28843058, 17491584,
    20894048, -28430026, 17407688, 20402437, -28026759, 17324989,
    19926868, -27632936, 17243457, 19466674, -27248250, 17163065,
    19021224, -26872408, 17083787, 18589914, -26505128, 17005598,
    18172173, -26146138, 16928473, 17767457, -25795180, 16852388,
    17375248, -25452003, 16777319, 16995055, -25116369, 16703245,
    16626406, -24788047, 16630143, 16268857, -24466817, 16557993,
    15921981, -24152465, 16486774, 15585371, -23844788, 16416466,
    15258640, -23543588, 16347050, 14941419, -23248676, 16278507,
    14633355, -22959870, 16210819, 14334110, -22676994, 16143969,
    14043363, -22399880, 16077938, 13760806, -22128364, 16012712,
    13486144, -21862290, 15948273, 13219097, -21601505, 15884606,
    12959394, -21345864, 15821695, 12706779, -21095225, 15759526,
};
/* ------------------------------------------------------------------ */
/* F_MUL - bit pattern float multiply (hgFloat.c:1280)                */
/* ------------------------------------------------------------------ */

HGuint32 F_MUL(HGuint32 a, HGuint32 b)
{
    /* operands: mantissa fields shifted into the top of the word with the
     * implicit 1 bit at position 31 -> product mantissas in [2^31, 2^32) */
    HGint32 sa, sb;
    HGuint32 prodHigh, sign, fraction, mantissa;
    HGint32 expA, expB, exponent;

    HG_ASSERT_AT("hgFloat.c", 1280, F_ISVALID(a) && F_ISVALID(b));
    sign = (b ^ a) & 0x80000000u;
    sa = (HGint32)(((a << 8) | 0x80000000u));
    sb = (HGint32)(((b << 8) | 0x80000000u));
    prodHigh = hgMulu64h((HGuint32)sa, (HGuint32)sb);

    expA = (HGint32)((2u * a) >> 24);
    if (expA == 0)
        expA = -256;
    expB = (HGint32)((2u * b) >> 24);
    exponent = (expB != 0) ? (expB + expA) : (expA - 256);
    exponent = ((HGint32)(prodHigh >> 31)) + exponent - 127;

    /* normalize the product: bit 31 set -> shift 1, else shift 2 */
    {
        HGint32 sp = (HGint32)prodHigh;
        HGuint32 sh = (sp < 0) ? 1u : 2u;
        fraction = ((HGuint32)sp << sh) >> 9;
    }
    mantissa = ((HGuint32)exponent << 23) | fraction;
    if (exponent >= 255)
        mantissa = 2139095039u;         /* +Inf */
    {
        HGuint32 result = (sign & 0x80000000u) | mantissa;
        if (exponent <= 0)
            result = 0;
        HG_ASSERT_AT("hgFloat.c", 1314, F_ISVALID(result));
        return result;
    }
}

/* ------------------------------------------------------------------ */
/* F_SQR - bit pattern float square (hgFloat.c:1684)                  */
/* ------------------------------------------------------------------ */

HGuint32 F_SQR(HGuint32 a)
{
    HGint32 v = (HGint32)hgMulu64h((a << 8) | 0x80000000u, (a << 8) | 0x80000000u);
    HGint32 exponent;
    HGuint32 mantissa;

    HG_ASSERT_AT("hgFloat.c", 1684, F_ISVALID(a));
    exponent = ((HGint32)(v >> 31)) + (HGint32)(((2u * a) >> 23) & 0xFFFFFFFEu) - 127;
    mantissa = ((HGuint32)v << ((v < 0) ? 1u : 2u)) >> 9;
    if (exponent <= 0)
    {
        mantissa = 0;
        exponent = 0;
    }
    {
        HGuint32 result = ((HGuint32)exponent << 23) | mantissa;
        if (exponent >= 255)
            result = 2139095039u;
        HG_ASSERT_AT("hgFloat.c", 1706, F_ISVALID(result));
        return result;
    }
}

/* ------------------------------------------------------------------ */
/* F_ITOF - integer to float on bit patterns (hgFloat.c)              */
/* ------------------------------------------------------------------ */

HGuint32 F_ITOF(HGint32 x)
{
    HGint32 mantissa = hgAbs32(x);
    HGint32 exponent = 8 - hgClz32((HGuint32)mantissa);
    HGuint32 m = lsrSigned((HGuint32)mantissa, exponent);
    HGuint32 res;
    exponent += 150;
    res = buildFloatUnchecked((HGuint32)(x & 0x80000000), exponent, m);
    if (exponent == 126)     /* zero input flushes to +0.0 */
        return 0;
    return res;
}

/* ------------------------------------------------------------------ */
/* F_RCP - reciprocal with Newton-Raphson refinement (hgFloat.c)      */
/* ------------------------------------------------------------------ */

HGuint32 F_RCP(HGuint32 a)
{
    HGint32 exponent = 254 - extractBiasedExponent(a);
    HGuint32 mantissa = a & 0x7FFFFFu;
    HGuint32 m, r, r1, rt;

    if (mantissa != 0)
    {
        HGuint32 scaled;
        --exponent;
        scaled = (mantissa | 0x800000u) << 8;
        r = s_rcpTable[(HGuint8)(scaled >> 23)];
        r1 = (r * (~(r * (scaled >> 8)) >> 16)) >> 7;
        r1 = r1 * (~(r1 * (scaled >> 16)) >> 15);   /* HIWORD(mantissaa) */
        rt = ~hgMulu64h(r1, scaled);
        m = hgMulu64h(r1, rt) >> 7;
    }
    else
    {
        m = mantissa;
    }
    return buildFloat(a & 0x80000000u, exponent, m);
}

/* ------------------------------------------------------------------ */
/* F_RSQ - inverse square root via lookup + refinement (hgFloat.c)    */
/* ------------------------------------------------------------------ */

HGuint32 F_RSQ(HGuint32 a)
{
    HGint32 biasedExp;
    HGint32 even;
    HGint32 exponent;
    const HGint32 *ix;
    HGint32 y;

    if (F_ZCMPLE(a))
        return 0;
    biasedExp = extractBiasedExponent(a);
    even = !((((HGint8)biasedExp) - 126) & 1);
    exponent = even + 126 - ((biasedExp - 126) >> 1);
    ix = &s_rsqlut[192 * even + 3 * (HGint32)((a & 0x7FFFFFu) >> 17)];
    y = ix[1] + hgMul64h(ix[0], (HGint32)(((a & 0x7FFFFFu) << 7) | 0x40000000u));
    return (((HGuint32)(ix[2] + hgMul64h(y, (HGint32)(((a & 0x7FFFFFu) << 7) | 0x40000000u)))
             >> (2 - 2 * even)) & 0x7FFFFFu)
           | ((HGuint32)exponent << 23);
}
