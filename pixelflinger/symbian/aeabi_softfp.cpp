/* aeabi_softfp.cpp - Self-contained ARM EABI (RTABI IHI0043D) IEEE-754
 * single/double-precision software floating-point helpers in pure integer C++.
 *
 * Uses strictly integer types (uint32_t / uint64_t) so GCC -mfloat-abi=soft
 * never emits recursive __aeabi_f* / __aeabi_d* calls, with zero WSD (.data/.bss)
 * and zero external Symbian DLL dependencies beyond ROM kernel DLLs.
 */

#include <stdint.h>

#ifndef PF_SOFTFP_PREFIX
#define PF_SOFTFP_NAME(fn) __aeabi_##fn
#else
#define PF_SOFTFP_CAT2(a, b) a##b
#define PF_SOFTFP_CAT(a, b)  PF_SOFTFP_CAT2(a, b)
#define PF_SOFTFP_NAME(fn)   PF_SOFTFP_CAT(PF_SOFTFP_PREFIX, fn)
#endif

static inline int pf_clz32(uint32_t x)
{
    return x ? __builtin_clz(x) : 32;
}

static inline int pf_clz64(uint64_t x)
{
    uint32_t hi = (uint32_t)(x >> 32);
    if (hi != 0u)
        return __builtin_clz(hi);
    uint32_t lo = (uint32_t)x;
    return lo ? (32 + __builtin_clz(lo)) : 64;
}

static inline bool pf_f_isnan(uint32_t a)
{
    return (a & 0x7FFFFFFFu) > 0x7F800000u;
}

static inline bool pf_d_isnan(uint64_t a)
{
    return (a & 0x7FFFFFFFFFFFFFFFULL) > 0x7FF0000000000000ULL;
}

/* Pack normalized/unnormalized significand `sig` where binary point is at bit 26
 * (i.e. normal [1.0, 2.0) has bit 26 set, bits [2:0] = {G, R, S}), with unbiased
 * exponent `exp` (where 1.0 * 2^0 has exp == 0). */
static uint32_t pf_pack_f32(uint32_t sign, int exp, uint32_t sig)
{
    if (sig == 0u)
        return sign;

    int lz = pf_clz32(sig) - 5; /* bit 26 -> clz == 5 */
    if (lz > 0) {
        sig <<= (lz < 31 ? lz : 31);
        exp -= lz;
    } else if (lz < 0) {
        int rsh = -lz;
        uint32_t sticky = (sig & ((1u << rsh) - 1u)) ? 1u : 0u;
        sig = (sig >> rsh) | sticky;
        exp += rsh;
    }

    int biased = exp + 127;
    if (biased >= 255)
        return sign | 0x7F800000u;

    if (biased <= 0) {
        int rsh = 1 - biased;
        if (rsh >= 29) {
            return sign;
        }
        uint32_t sticky = (sig & ((1u << rsh) - 1u)) ? 1u : 0u;
        sig = (sig >> rsh) | sticky;
        biased = 0;
    }

    uint32_t grs = sig & 7u;
    uint32_t mant = sig >> 3; /* bit 23 = hidden 1 when biased > 0 */
    if (grs > 4u || (grs == 4u && (mant & 1u))) {
        ++mant;
        if (mant >= (1u << 24)) {
            mant >>= 1;
            ++biased;
            if (biased >= 255)
                return sign | 0x7F800000u;
        } else if (biased == 0 && (mant & (1u << 23))) {
            biased = 1;
        }
    }

    if (biased == 0)
        return sign | (mant & 0x7FFFFFu);
    return sign | ((uint32_t)biased << 23) | (mant & 0x7FFFFFu);
}

/* Pack double where normal [1.0, 2.0) has bit 55 set, bits [2:0] = {G, R, S} */
static uint64_t pf_pack_f64(uint64_t sign, int exp, uint64_t sig)
{
    if (sig == 0ULL)
        return sign;

    int lz = pf_clz64(sig) - 8; /* bit 55 -> clz64 == 8 */
    if (lz > 0) {
        sig <<= (lz < 63 ? lz : 63);
        exp -= lz;
    } else if (lz < 0) {
        int rsh = -lz;
        uint64_t sticky = (sig & ((1ULL << rsh) - 1ULL)) ? 1ULL : 0ULL;
        sig = (sig >> rsh) | sticky;
        exp += rsh;
    }

    int biased = exp + 1023;
    if (biased >= 2047)
        return sign | 0x7FF0000000000000ULL;

    if (biased <= 0) {
        int rsh = 1 - biased;
        if (rsh >= 58)
            return sign;
        uint64_t sticky = (sig & ((1ULL << rsh) - 1ULL)) ? 1ULL : 0ULL;
        sig = (sig >> rsh) | sticky;
        biased = 0;
    }

    uint64_t grs = sig & 7ULL;
    uint64_t mant = sig >> 3; /* bit 52 = hidden 1 when biased > 0 */
    if (grs > 4ULL || (grs == 4ULL && (mant & 1ULL))) {
        ++mant;
        if (mant >= (1ULL << 53)) {
            mant >>= 1;
            ++biased;
            if (biased >= 2047)
                return sign | 0x7FF0000000000000ULL;
        } else if (biased == 0 && (mant & (1ULL << 52))) {
            biased = 1;
        }
    }

    if (biased == 0)
        return sign | (mant & 0x000FFFFFFFFFFFFFULL);
    return sign | ((uint64_t)biased << 52) | (mant & 0x000FFFFFFFFFFFFFULL);
}

extern "C" {

/* --- Single-precision arithmetic ------------------------------------------ */

uint32_t PF_SOFTFP_NAME(fadd)(uint32_t a, uint32_t b)
{
    if (pf_f_isnan(a) || pf_f_isnan(b))
        return 0x7FC00000u;

    uint32_t sa = a & 0x80000000u, sb = b & 0x80000000u;
    uint32_t aa = a & 0x7FFFFFFFu, ab = b & 0x7FFFFFFFu;

    if (aa < ab) {
        uint32_t t = a; a = b; b = t;
        t = sa; sa = sb; sb = t;
        t = aa; aa = ab; ab = t;
    }

    if (aa == 0x7F800000u) {
        if (ab == 0x7F800000u && sa != sb)
            return 0x7FC00000u;
        return a;
    }
    if (ab == 0u) {
        if (aa == 0u)
            return sa & sb; /* -0.0 + -0.0 = -0.0, else +0.0 */
        return a;
    }

    int ea = (int)((aa >> 23) & 0xFFu);
    int eb = (int)((ab >> 23) & 0xFFu);
    uint32_t ma = (aa & 0x7FFFFFu) << 3;
    uint32_t mb = (ab & 0x7FFFFFu) << 3;
    if (ea == 0) { ea = 1; } else { ma |= (1u << 26); }
    if (eb == 0) { eb = 1; } else { mb |= (1u << 26); }

    int diff = ea - eb;
    if (diff > 0) {
        if (diff >= 29) {
            mb = 1u;
        } else {
            uint32_t sticky = (mb & ((1u << diff) - 1u)) ? 1u : 0u;
            mb = (mb >> diff) | sticky;
        }
    }

    uint32_t sig = (sa == sb) ? (ma + mb) : (ma - mb);
    if (sig == 0u)
        return 0u;
    return pf_pack_f32(sa, ea - 127, sig);
}

uint32_t PF_SOFTFP_NAME(fsub)(uint32_t a, uint32_t b)
{
    return PF_SOFTFP_NAME(fadd)(a, b ^ 0x80000000u);
}

uint32_t PF_SOFTFP_NAME(fmul)(uint32_t a, uint32_t b)
{
    if (pf_f_isnan(a) || pf_f_isnan(b))
        return 0x7FC00000u;

    uint32_t sign = (a ^ b) & 0x80000000u;
    uint32_t aa = a & 0x7FFFFFFFu, ab = b & 0x7FFFFFFFu;

    if (aa == 0x7F800000u || ab == 0x7F800000u) {
        if (aa == 0u || ab == 0u)
            return 0x7FC00000u;
        return sign | 0x7F800000u;
    }
    if (aa == 0u || ab == 0u)
        return sign;

    int ea = (int)(aa >> 23), eb = (int)(ab >> 23);
    uint32_t ma = aa & 0x7FFFFFu, mb = ab & 0x7FFFFFu;
    if (ea == 0) {
        int sh = pf_clz32(ma) - 8;
        ma <<= sh;
        ea = 1 - sh;
    } else {
        ma |= (1u << 23);
    }
    if (eb == 0) {
        int sh = pf_clz32(mb) - 8;
        mb <<= sh;
        eb = 1 - sh;
    } else {
        mb |= (1u << 23);
    }

    int exp = (ea - 127) + (eb - 127);
    uint64_t prod = (uint64_t)ma * (uint64_t)mb; /* [2^46, 2^48) */
    if (prod & (1ULL << 47)) {
        uint32_t sticky = (prod & ((1ULL << 21) - 1ULL)) ? 1u : 0u;
        uint32_t sig = (uint32_t)(prod >> 21) | sticky;
        return pf_pack_f32(sign, exp + 1, sig);
    } else {
        uint32_t sticky = (prod & ((1ULL << 20) - 1ULL)) ? 1u : 0u;
        uint32_t sig = (uint32_t)(prod >> 20) | sticky;
        return pf_pack_f32(sign, exp, sig);
    }
}

uint32_t PF_SOFTFP_NAME(fdiv)(uint32_t a, uint32_t b)
{
    if (pf_f_isnan(a) || pf_f_isnan(b))
        return 0x7FC00000u;

    uint32_t sign = (a ^ b) & 0x80000000u;
    uint32_t aa = a & 0x7FFFFFFFu, ab = b & 0x7FFFFFFFu;

    if (aa == 0x7F800000u) {
        if (ab == 0x7F800000u)
            return 0x7FC00000u;
        return sign | 0x7F800000u;
    }
    if (ab == 0u) {
        if (aa == 0u)
            return 0x7FC00000u;
        return sign | 0x7F800000u;
    }
    if (aa == 0u || ab == 0x7F800000u)
        return sign;

    int ea = (int)(aa >> 23), eb = (int)(ab >> 23);
    uint32_t ma = aa & 0x7FFFFFu, mb = ab & 0x7FFFFFu;
    if (ea == 0) {
        int sh = pf_clz32(ma) - 8;
        ma <<= sh;
        ea = 1 - sh;
    } else {
        ma |= (1u << 23);
    }
    if (eb == 0) {
        int sh = pf_clz32(mb) - 8;
        mb <<= sh;
        eb = 1 - sh;
    } else {
        mb |= (1u << 23);
    }

    int exp = (ea - 127) - (eb - 127);
    if (ma < mb) {
        ma <<= 1;
        --exp;
    }
    uint32_t rem = ma;
    uint32_t q = 0u;
    for (int i = 26; i >= 0; --i) {
        if (rem >= mb) {
            rem -= mb;
            q |= (1u << i);
        }
        rem <<= 1;
    }
    if (rem != 0u)
        q |= 1u;
    return pf_pack_f32(sign, exp, q);
}

/* --- Conversions ---------------------------------------------------------- */

int32_t PF_SOFTFP_NAME(f2iz)(uint32_t a)
{
    if (pf_f_isnan(a))
        return 0;
    uint32_t sign = a >> 31;
    int exp = (int)((a >> 23) & 0xFFu) - 127;
    if (exp < 0)
        return 0;
    if (exp >= 31)
        return sign ? (-2147483647 - 1) : 2147483647;
    uint32_t m = (1u << 23) | (a & 0x7FFFFFu);
    uint32_t mag = (exp >= 23) ? (m << (exp - 23)) : (m >> (23 - exp));
    return sign ? -(int32_t)mag : (int32_t)mag;
}

uint32_t PF_SOFTFP_NAME(ui2f)(uint32_t a)
{
    if (a == 0u)
        return 0u;
    int lz = pf_clz32(a);
    int exp = 31 - lz;
    uint32_t sig;
    if (exp <= 26) {
        sig = a << (26 - exp);
    } else {
        int rsh = exp - 26;
        uint32_t sticky = (a & ((1u << rsh) - 1u)) ? 1u : 0u;
        sig = (a >> rsh) | sticky;
    }
    return pf_pack_f32(0u, exp, sig);
}

uint32_t PF_SOFTFP_NAME(i2f)(int32_t a)
{
    if (a == 0)
        return 0u;
    uint32_t sign = (a < 0) ? 0x80000000u : 0u;
    uint32_t mag = (a < 0) ? (0u - (uint32_t)a) : (uint32_t)a;
    return sign | PF_SOFTFP_NAME(ui2f)(mag);
}

/* --- Single-precision comparisons (RTABI boolean: 1 if true, 0 if false) -- */

int PF_SOFTFP_NAME(fcmpun)(uint32_t a, uint32_t b)
{
    return (pf_f_isnan(a) || pf_f_isnan(b)) ? 1 : 0;
}

int PF_SOFTFP_NAME(fcmpeq)(uint32_t a, uint32_t b)
{
    if (pf_f_isnan(a) || pf_f_isnan(b))
        return 0;
    if (((a | b) & 0x7FFFFFFFu) == 0u)
        return 1;
    return (a == b) ? 1 : 0;
}

int PF_SOFTFP_NAME(fcmplt)(uint32_t a, uint32_t b)
{
    if (pf_f_isnan(a) || pf_f_isnan(b))
        return 0;
    if (((a | b) & 0x7FFFFFFFu) == 0u)
        return 0;
    uint32_t sa = a >> 31, sb = b >> 31;
    if (sa != sb)
        return sa ? 1 : 0;
    return sa ? ((a > b) ? 1 : 0) : ((a < b) ? 1 : 0);
}

int PF_SOFTFP_NAME(fcmple)(uint32_t a, uint32_t b)
{
    if (pf_f_isnan(a) || pf_f_isnan(b))
        return 0;
    if (((a | b) & 0x7FFFFFFFu) == 0u)
        return 1;
    return (a == b || PF_SOFTFP_NAME(fcmplt)(a, b)) ? 1 : 0;
}

int PF_SOFTFP_NAME(fcmpgt)(uint32_t a, uint32_t b)
{
    return PF_SOFTFP_NAME(fcmplt)(b, a);
}

int PF_SOFTFP_NAME(fcmpge)(uint32_t a, uint32_t b)
{
    return PF_SOFTFP_NAME(fcmple)(b, a);
}

/* --- Double-precision helpers (for matrix invert & spotlight attenuation) - */

uint64_t PF_SOFTFP_NAME(f2d)(uint32_t a)
{
    uint64_t sign = (uint64_t)(a & 0x80000000u) << 32;
    uint32_t exp = (a >> 23) & 0xFFu;
    uint32_t frac = a & 0x7FFFFFu;

    if (exp == 0xFFu) {
        if (frac != 0u)
            return sign | 0x7FF8000000000000ULL | ((uint64_t)frac << 29);
        return sign | 0x7FF0000000000000ULL;
    }
    if (exp == 0u) {
        if (frac == 0u)
            return sign;
        int sh = pf_clz32(frac) - 8;
        frac = (frac << sh) & 0x7FFFFFu;
        uint64_t dexp = (uint64_t)(1023 - 126 - sh);
        return sign | (dexp << 52) | ((uint64_t)frac << 29);
    }
    uint64_t dexp = (uint64_t)(exp - 127 + 1023);
    return sign | (dexp << 52) | ((uint64_t)frac << 29);
}

uint32_t PF_SOFTFP_NAME(d2f)(uint64_t a)
{
    uint32_t sign = (uint32_t)(a >> 63) << 31;
    uint32_t exp = (uint32_t)((a >> 52) & 0x7FFULL);
    uint64_t frac = a & 0x000FFFFFFFFFFFFFULL;

    if (exp == 0x7FFu) {
        if (frac != 0ULL)
            return sign | 0x7FC00000u;
        return sign | 0x7F800000u;
    }
    if (exp == 0u)
        return sign; /* double subnormals all underflow to signed zero in float */

    int uexp = (int)exp - 1023;
    uint64_t m53 = (1ULL << 52) | frac;
    uint32_t sticky = (m53 & ((1ULL << 26) - 1ULL)) ? 1u : 0u;
    uint32_t sig = (uint32_t)(m53 >> 26) | sticky;
    return pf_pack_f32(sign, uexp, sig);
}

uint64_t PF_SOFTFP_NAME(i2d)(int32_t a)
{
    if (a == 0)
        return 0ULL;
    uint64_t sign = (a < 0) ? (1ULL << 63) : 0ULL;
    uint32_t mag = (a < 0) ? (0u - (uint32_t)a) : (uint32_t)a;
    int lz = pf_clz32(mag);
    int exp = 31 - lz;
    uint64_t frac = ((uint64_t)mag << (52 - exp)) & 0x000FFFFFFFFFFFFFULL;
    return sign | ((uint64_t)(exp + 1023) << 52) | frac;
}

uint64_t PF_SOFTFP_NAME(dadd)(uint64_t a, uint64_t b)
{
    if (pf_d_isnan(a) || pf_d_isnan(b))
        return 0x7FF8000000000000ULL;

    uint64_t sa = a & (1ULL << 63), sb = b & (1ULL << 63);
    uint64_t aa = a & 0x7FFFFFFFFFFFFFFFULL, ab = b & 0x7FFFFFFFFFFFFFFFULL;

    if (aa < ab) {
        uint64_t t = a; a = b; b = t;
        t = sa; sa = sb; sb = t;
        t = aa; aa = ab; ab = t;
    }

    if (aa == 0x7FF0000000000000ULL) {
        if (ab == 0x7FF0000000000000ULL && sa != sb)
            return 0x7FF8000000000000ULL;
        return a;
    }
    if (ab == 0ULL) {
        if (aa == 0ULL)
            return sa & sb;
        return a;
    }

    int ea = (int)((aa >> 52) & 0x7FFULL);
    int eb = (int)((ab >> 52) & 0x7FFULL);
    uint64_t ma = (aa & 0x000FFFFFFFFFFFFFULL) << 3;
    uint64_t mb = (ab & 0x000FFFFFFFFFFFFFULL) << 3;
    if (ea == 0) { ea = 1; } else { ma |= (1ULL << 55); }
    if (eb == 0) { eb = 1; } else { mb |= (1ULL << 55); }

    int diff = ea - eb;
    if (diff > 0) {
        if (diff >= 58) {
            mb = 1ULL;
        } else {
            uint64_t sticky = (mb & ((1ULL << diff) - 1ULL)) ? 1ULL : 0ULL;
            mb = (mb >> diff) | sticky;
        }
    }

    uint64_t sig = (sa == sb) ? (ma + mb) : (ma - mb);
    if (sig == 0ULL)
        return 0ULL;
    return pf_pack_f64(sa, ea - 1023, sig);
}

uint64_t PF_SOFTFP_NAME(dsub)(uint64_t a, uint64_t b)
{
    return PF_SOFTFP_NAME(dadd)(a, b ^ (1ULL << 63));
}

static inline void pf_mul64x64(uint64_t a, uint64_t b, uint64_t* hi_out, uint64_t* lo_out)
{
    uint64_t a0 = (uint32_t)a, a1 = a >> 32;
    uint64_t b0 = (uint32_t)b, b1 = b >> 32;
    uint64_t p00 = a0 * b0;
    uint64_t p01 = a0 * b1;
    uint64_t p10 = a1 * b0;
    uint64_t p11 = a1 * b1;
    uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
    *hi_out = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    *lo_out = (mid << 32) | (uint32_t)p00;
}

uint64_t PF_SOFTFP_NAME(dmul)(uint64_t a, uint64_t b)
{
    if (pf_d_isnan(a) || pf_d_isnan(b))
        return 0x7FF8000000000000ULL;

    uint64_t sign = (a ^ b) & (1ULL << 63);
    uint64_t aa = a & 0x7FFFFFFFFFFFFFFFULL, ab = b & 0x7FFFFFFFFFFFFFFFULL;

    if (aa == 0x7FF0000000000000ULL || ab == 0x7FF0000000000000ULL) {
        if (aa == 0ULL || ab == 0ULL)
            return 0x7FF8000000000000ULL;
        return sign | 0x7FF0000000000000ULL;
    }
    if (aa == 0ULL || ab == 0ULL)
        return sign;

    int ea = (int)(aa >> 52), eb = (int)(ab >> 52);
    uint64_t ma = aa & 0x000FFFFFFFFFFFFFULL, mb = ab & 0x000FFFFFFFFFFFFFULL;
    if (ea == 0) {
        int sh = pf_clz64(ma) - 11;
        ma <<= sh;
        ea = 1 - sh;
    } else {
        ma |= (1ULL << 52);
    }
    if (eb == 0) {
        int sh = pf_clz64(mb) - 11;
        mb <<= sh;
        eb = 1 - sh;
    } else {
        mb |= (1ULL << 52);
    }

    int exp = (ea - 1023) + (eb - 1023);
    /* Shift ma, mb left by 11 -> [2^63, 2^64), product in [2^126, 2^128) */
    uint64_t hi, lo;
    pf_mul64x64(ma << 11, mb << 11, &hi, &lo);
    if (hi & (1ULL << 63)) {
        uint64_t sticky = (lo != 0ULL || (hi & 0xFFULL) != 0ULL) ? 1ULL : 0ULL;
        uint64_t sig = (hi >> 8) | sticky;
        return pf_pack_f64(sign, exp + 1, sig);
    } else {
        uint64_t sticky = (lo != 0ULL || (hi & 0x7FULL) != 0ULL) ? 1ULL : 0ULL;
        uint64_t sig = (hi >> 7) | sticky;
        return pf_pack_f64(sign, exp, sig);
    }
}

/* Double division is used by the inverse modelview (lighting) path. */
uint64_t PF_SOFTFP_NAME(ddiv)(uint64_t a, uint64_t b)
{
    if (pf_d_isnan(a) || pf_d_isnan(b))
        return 0x7FF8000000000000ULL;
    const uint64_t sign = (a ^ b) & 0x8000000000000000ULL;
    const uint64_t aa = a & 0x7FFFFFFFFFFFFFFFULL;
    const uint64_t ab = b & 0x7FFFFFFFFFFFFFFFULL;
    const uint64_t inf = 0x7FF0000000000000ULL;
    if (aa == inf)
        return ab == inf ? 0x7FF8000000000000ULL : (sign | inf);
    if (ab == 0)
        return aa == 0 ? 0x7FF8000000000000ULL : (sign | inf);
    if (aa == 0 || ab == inf)
        return sign;
    int ea = int(aa >> 52), eb = int(ab >> 52);
    uint64_t ma = aa & 0x000FFFFFFFFFFFFFULL;
    uint64_t mb = ab & 0x000FFFFFFFFFFFFFULL;
    if (!ea) {
        const int sh = pf_clz64(ma) - 11;
        ma <<= sh;
        ea = 1 - sh;
    } else {
        ma |= 1ULL << 52;
    }
    if (!eb) {
        const int sh = pf_clz64(mb) - 11;
        mb <<= sh;
        eb = 1 - sh;
    } else {
        mb |= 1ULL << 52;
    }
    int exp = ea - eb;
    if (ma < mb) {
        ma <<= 1;
        --exp;
    }
    uint64_t rem = ma, q = 0;
    for (int i = 55; i >= 0; --i) {
        if (rem >= mb) {
            rem -= mb;
            q |= 1ULL << i;
        }
        rem <<= 1;
    }
    if (rem) q |= 1;
    return pf_pack_f64(sign, exp, q);
}

int PF_SOFTFP_NAME(dcmplt)(uint64_t a, uint64_t b)
{
    if (pf_d_isnan(a) || pf_d_isnan(b))
        return 0;
    if (((a | b) & 0x7FFFFFFFFFFFFFFFULL) == 0ULL)
        return 0;
    uint64_t sa = a >> 63, sb = b >> 63;
    if (sa != sb)
        return sa ? 1 : 0;
    return sa ? ((a > b) ? 1 : 0) : ((a < b) ? 1 : 0);
}

int PF_SOFTFP_NAME(dcmpgt)(uint64_t a, uint64_t b)
{
    return PF_SOFTFP_NAME(dcmplt)(b, a);
}

#ifndef PF_SOFTFP_PREFIX
uint64_t pf_udivmod64(uint64_t n, uint64_t d, uint64_t* rem_out)
{
    if (d == 0ULL) {
        if (rem_out) *rem_out = 0ULL;
        return 0ULL;
    }
    if ((n >> 32) == 0ULL && (d >> 32) == 0ULL) {
        uint32_t n32 = (uint32_t)n;
        uint32_t d32 = (uint32_t)d;
        if (rem_out) *rem_out = (uint64_t)(n32 % d32);
        return (uint64_t)(n32 / d32);
    }
    if (n < d) {
        if (rem_out) *rem_out = n;
        return 0ULL;
    }
    int shift = pf_clz64(d) - pf_clz64(n);
    uint64_t den = d << shift;
    uint64_t rem = n;
    uint64_t quot = 0ULL;
    for (int i = shift; i >= 0; --i) {
        quot <<= 1;
        if (rem >= den) {
            rem -= den;
            quot |= 1ULL;
        }
        den >>= 1;
    }
    if (rem_out) *rem_out = rem;
    return quot;
}

int64_t pf_sdivmod64(int64_t n, int64_t d, int64_t* rem_out)
{
    int neg_q = ((n ^ d) < 0) ? 1 : 0;
    int neg_r = (n < 0) ? 1 : 0;
    uint64_t un = (n < 0) ? (0ULL - (uint64_t)n) : (uint64_t)n;
    uint64_t ud = (d < 0) ? (0ULL - (uint64_t)d) : (uint64_t)d;
    uint64_t urem = 0ULL;
    uint64_t uq = pf_udivmod64(un, ud, &urem);
    if (rem_out)
        *rem_out = neg_r ? -(int64_t)urem : (int64_t)urem;
    return neg_q ? -(int64_t)uq : (int64_t)uq;
}

int __aeabi_idiv0(int return_value)
{
    return return_value;
}

long long __aeabi_ldiv0(long long return_value)
{
    return return_value;
}
#endif

} /* extern "C" */
