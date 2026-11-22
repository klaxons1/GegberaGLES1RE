/* test_imath.c - unit tests for the imath scalar layer.
 *
 * Expected bit patterns computed independently (IEEE semantics / the
 * documented fixed point domains), not by running the decompiled code.
 */
#include "imath/imApi.h"
#include "hg/hgInt32.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static int g_total = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_total;                                                           \
        if (!(cond)) {                                                       \
            ++g_fail;                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while (0)

#define CHECK_EQ(expr, expected)                                             \
    do {                                                                     \
        unsigned int _v = (unsigned int)(expr);                              \
        unsigned int _e = (unsigned int)(expected);                          \
        ++g_total;                                                           \
        if (_v != _e) {                                                      \
            ++g_fail;                                                        \
            fprintf(stderr, "FAIL %s:%d: %s == 0x%08x, got 0x%08x\n",        \
                    __FILE__, __LINE__, #expr, _e, _v);                      \
        }                                                                    \
    } while (0)

#define FIN(x) hgFloatBitPattern(x)

static void test_construct(void)
{
    CHECK_EQ(IScalar_zero(), 0);
    CHECK_EQ(IScalar_one(), FIN(1.0f));
    CHECK_EQ(IScalar_intToScalar(5), FIN(5.0f));
    CHECK_EQ(IScalar_intToScalar(-5), FIN(-5.0f));
    CHECK_EQ(IScalar_intToScalar(0), 0);
    CHECK_EQ(IScalar_immediate(2.5f), FIN(2.5f));
    CHECK_EQ(IScalar_ftos(2.5f), FIN(2.5f));
    CHECK(IScalar_stof(FIN(3.25f)) == 3.25f);
    /* denormal flush */
    CHECK_EQ(IScalar_iftos(0x007FFFFF), 0);
    CHECK_EQ(IScalar_iftos(0x00000001), 0);
    /* NaN/Inf: original keeps the sign byte, forces mantissa 0x7FFFFF */
    CHECK_EQ(IScalar_iftos(0x7FC00000), (HGint32)0x7F7FFFFFu);
    CHECK_EQ(IScalar_iftos(0xFF800000), (HGint32)0xFF7FFFFFu);
    CHECK_EQ(IScalar_iftos(FIN(1.5f)), FIN(1.5f));

    CHECK_EQ(IScalar_set(129, 0x28000000), FIN(5.0f));
    CHECK_EQ(IScalar_set(127, 0x20000000), FIN(1.0f));
    CHECK_EQ(IScalar_set(126, 0x20000000), FIN(0.5f));
    CHECK_EQ(IScalar_set(127, -0x20000000), FIN(-1.0f));
    CHECK_EQ(IScalar_set(0, 0), 0);
}

static void test_accessors(void)
{
    CHECK_EQ(IScalar_getExponent(FIN(5.0f)), 129);
    CHECK_EQ(IScalar_getExponent(FIN(1.0f)), 127);
    CHECK_EQ(IScalar_getExponent(FIN(0.5f)), 126);
    /* 6.26 mantissa: 1.x is 2^29 .. 2^30 */
    CHECK_EQ(IScalar_getMantissa(FIN(1.0f)), 0x20000000);
    CHECK_EQ(IScalar_getMantissa(FIN(1.5f)), 0x30000000);
    CHECK_EQ(IScalar_getMantissa(FIN(-1.5f)), -0x30000000);
    CHECK_EQ(IScalar_getMantissa(FIN(0.5f)), 0x20000000);
    CHECK_EQ(IScalar_getUnsignedMantissa(FIN(1.5f)), 0x30000000u);
    CHECK_EQ(IScalar_getOESExponent(FIN(2.0f)), 128 - 140);
    CHECK_EQ(IScalar_getOESMantissa(FIN(2.0f)), 0x20000000);

    CHECK_EQ(IScalar_toUnitScalar(FIN(0.5f)), 0x10000000);
    CHECK_EQ(IScalar_toUnitScalar(FIN(-0.5f)), -0x10000000);
    CHECK_EQ(IScalar_toUnitScalar(FIN(1.5f)), 0x20000000);   /* clamp */
    CHECK_EQ(IScalar_toUnitScalar(FIN(-2.0f)), -0x20000000); /* clamp */
    CHECK_EQ(IScalar_toUnitScalar(FIN(0.25f)), 0x08000000);
}

static void test_fixed(void)
{
    /* fixedToScalar(m, n) = scalar(m * 2^-n) */
    CHECK_EQ(IScalar_fixedToScalar(60, 4), FIN(3.75f));
    CHECK_EQ(IScalar_fixedToScalar(-16, 4), FIN(-1.0f));
    CHECK_EQ(IScalar_fixedToScalar(-32, 4), FIN(-2.0f));
    CHECK_EQ(IScalar_fixedToScalar(0, 4), 0);
    CHECK_EQ(IScalar_fixedToScalar(1, 0), FIN(1.0f));
    CHECK_EQ(IScalar_fixedToScalar(0x80000000, 30), FIN(-2.0f)); /* INT_MIN path */

    /* scalarToFixed(r, n) = trunc(r * 2^n) (arithmetic shift: -1.5 -> -2) */
    CHECK_EQ(IScalar_scalarToFixed(FIN(1.0f), 0), 1);
    CHECK_EQ(IScalar_scalarToFixed(FIN(1.5f), 0), 1);
    CHECK_EQ(IScalar_scalarToFixed(FIN(3.75f), 4), 60);
    CHECK_EQ(IScalar_scalarToFixed(FIN(255.0f), 0), 255);
    CHECK_EQ(IScalar_scalarToFixed(FIN(0.5f), 1), 1);
    CHECK_EQ(IScalar_scalarToFixed(FIN(-1.5f), 0), -2);
    CHECK_EQ(IScalar_scalarToFixed(FIN(2.5f), 1), 5);

    /* byte conversion */
    CHECK_EQ(IScalar_byteToScalar(255), FIN(1.0f));
    CHECK_EQ(IScalar_byteToScalar(0), 0);
    CHECK_EQ(IScalar_byteToScalar(128), 0x3F008081u);   /* 128/255 (original carry) */
    {
        float v = IScalar_stof(IScalar_byteToScalar(200));
        CHECK(fabs(v - 200.0f / 255.0f) < 1e-6f);
    }
    CHECK_EQ(IScalar_toFixedHO(FIN(1.0f), 8), 255);
}

static void test_arith(void)
{
    CHECK_EQ(IScalar_mul(FIN(2.0f), FIN(3.0f)), FIN(6.0f));
    CHECK_EQ(IScalar_mul(FIN(-2.5f), FIN(4.0f)), FIN(-10.0f));
    CHECK_EQ(IScalar_mul(FIN(0.0f), FIN(100.0f)), FIN(0.0f));
    CHECK_EQ(IScalar_add(FIN(1.5f), FIN(2.25f)), FIN(3.75f));
    CHECK_EQ(IScalar_sub(FIN(5.0f), FIN(3.0f)), FIN(2.0f));
    CHECK_EQ(IScalar_sqr(FIN(4.0f)), FIN(16.0f));
    CHECK_EQ(IScalar_rcp(FIN(4.0f)), FIN(0.25f));
    CHECK_EQ(IScalar_rcp(FIN(1.0f)), FIN(1.0f));
    {
        /* rcp is an estimate: exact for powers of two, else within 1 ulp */
        float r = IScalar_stof(IScalar_rcp(FIN(3.0f)));
        CHECK(fabs(r - 1.0f / 3.0f) < 1e-6f);
        r = IScalar_stof(IScalar_rcp(FIN(10.0f)));
        CHECK(fabs(r - 0.1f) < 1e-6f);
    }
    {
        /* rsq: polynomial estimate, <= 1 ulp error (0.5 itself misses the
         * binade boundary: F_RSQ(4.0) = 0.49999995 - faithful) */
        float r = IScalar_stof(IScalar_rsq(FIN(4.0f)));
        CHECK(fabs(r - 0.5f) < 1e-6f);
        r = IScalar_stof(IScalar_rsq(FIN(2.0f)));
        CHECK(fabs(r - 1.0f / sqrtf(2.0f)) < 1e-6f);
        r = IScalar_stof(IScalar_rsq(FIN(1.0f)));
        CHECK(fabs(r - 1.0f) < 1e-6f);
    }
    CHECK_EQ(IScalar_negate(FIN(1.5f)), FIN(-1.5f));
    CHECK_EQ(IScalar_abs(FIN(-1.5f)), FIN(1.5f));
    CHECK_EQ(IScalar_max(FIN(1.5f), FIN(2.5f)), FIN(2.5f));
    CHECK_EQ(IScalar_max(FIN(-9.0f), FIN(-2.0f)), FIN(-2.0f));
    {
        float r = IScalar_stof(IScalar_degreesToRadians(FIN(180.0f)));
        CHECK(fabs(r - 3.14159265f) < 1e-6f);
        r = IScalar_stof(IScalar_degreesToRadians(FIN(90.0f)));
        CHECK(fabs(r - 1.5707963f) < 1e-6f);
    }
    CHECK_EQ(IScalar_cos(FIN(0.0f)), FIN(1.0f));
}

static void test_compares(void)
{
    CHECK_EQ(IScalar_cmpeq(FIN(1.0f), FIN(1.0f)), 1);
    CHECK_EQ(IScalar_cmpeq(FIN(1.0f), FIN(2.0f)), 0);
    CHECK_EQ(IScalar_cmpne(FIN(1.0f), FIN(2.0f)), 1);
    CHECK_EQ(IScalar_cmplt(FIN(1.0f), FIN(2.0f)), 1);
    CHECK_EQ(IScalar_cmplt(FIN(-2.0f), FIN(-1.0f)), 1);
    CHECK_EQ(IScalar_cmpgt(FIN(2.0f), FIN(1.0f)), 1);
    CHECK_EQ(IScalar_cmple(FIN(1.0f), FIN(1.0f)), 1);
    CHECK_EQ(IScalar_cmpge(FIN(1.0f), FIN(0.5f)), 1);

    CHECK_EQ(IScalar_testeq(0), 1);
    CHECK_EQ(IScalar_testeq(FIN(1.0f)), 0);
    CHECK_EQ(IScalar_testne(FIN(1.0f)), 1);
    CHECK_EQ(IScalar_testlt(FIN(-1.0f)), 1);
    CHECK_EQ(IScalar_testlt(FIN(1.0f)), 0);
    CHECK_EQ(IScalar_testgt(FIN(1.0f)), 1);
    CHECK_EQ(IScalar_testle(FIN(0.0f)), 1);
    CHECK_EQ(IScalar_testge(FIN(0.0f)), 1);
    CHECK_EQ(IScalar_testge(0x80000000u), 1);   /* -0.0 */
}

static void test_clamp_setnorm(void)
{
    CHECK_EQ(IScalar_clampUnit(FIN(2.0f)), FIN(1.0f));
    CHECK_EQ(IScalar_clampUnit(FIN(-2.0f)), FIN(0.0f));
    CHECK_EQ(IScalar_clampUnit(FIN(0.25f)), FIN(0.25f));

    CHECK_EQ(IScalar_setAndNormalize(128, 0x30000000), FIN(3.0f));
    CHECK_EQ(IScalar_setAndNormalize(127, 0x20000000), FIN(1.0f));
}

static void test_unit_scalar(void)
{
    CHECK_EQ(IUnitScalar_maxZero(0x20000000), 0x20000000);
    CHECK_EQ(IUnitScalar_maxZero(-1), 0);

    CHECK_EQ(IUnitScalar_lerp(0, 0x20000000, 0x10000000), 0x10000000);
    CHECK_EQ(IUnitScalar_lerp(0x20000000, 0x20000000, 0x10000000), 0x20000000);
    CHECK_EQ(IUnitScalar_lerp(0x20000000, 0, 0x20000000), 0);
    CHECK_EQ(IUnitScalar_lerp(0x10000000, 0x20000000, 0x10000000), 0x18000000);

    CHECK_EQ(IUnitScalar_tos(0x10000000), FIN(0.5f));
    CHECK_EQ(IUnitScalar_tos(0x20000000), FIN(1.0f));
    CHECK_EQ(IUnitScalar_tos(-0x20000000), FIN(-1.0f));
    CHECK_EQ(IUnitScalar_tos(0), 0);
    CHECK_EQ(IUnitScalar_tos(0x08000000), FIN(0.25f));

    CHECK_EQ(IUnitScalar_isValid(0x20000000), 1);
    CHECK_EQ(IScalar_isValid(FIN(1.5f)), 1);
    CHECK_EQ(IScalar_isValid(FIN(0.25f)), 1);
    CHECK_EQ(IScalar_isValid(0), 1);
    CHECK_EQ(IScalar_isValid(0x7F800000u), 0);  /* Inf: exponent 255 */
}

static void test_log2(void)
{
    CHECK_EQ(XScalar_log2(0), (HGint32)0x80000000u);
    CHECK_EQ((HGuint32)XScalar_log2(0x80000000), 0x80000000u);
    /* result = (log2(y) - 21) << 21 */
    CHECK_EQ(XScalar_log2(1 << 30), 9 << 21);
    CHECK_EQ(XScalar_log2(1 << 20), -(1 << 21));
    {
        HGint32 r = XScalar_log2(3 << 28);
        /* log2(3<<28) - 21 = 8.58496 */
        HGint32 want = (HGint32)(8.58496f * 2097152);
        CHECK(r > want - 8 && r < want + 8);
    }
}

int main(void)
{
    test_construct();
    test_accessors();
    test_fixed();
    test_arith();
    test_compares();
    test_clamp_setnorm();
    test_unit_scalar();
    test_log2();

    printf("imath tests: %d checks, %d failures\n", g_total, g_fail);
    return g_fail ? 1 : 0;
}
