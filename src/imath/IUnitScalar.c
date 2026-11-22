/* IUnitScalar.c - unit scalar (2.30 fixed point, [-1, 1)) operations.
 *
 * Clean port of the decompiled IUnitScalar.c from the Gerbera GLES1 driver.
 * IUnitScalar_sin/cos are defined in hg/hgFloat.c - the reference binary
 * asserts into hgFloat.c from those functions, pinning them there.
 */
#include "imath/imApi.h"
#include "hg/hgInt32.h"

HGint32 IUnitScalar_maxZero(HGint32 x)
{
    return hgMaxZero32(x);
}

HGint32 IUnitScalar_lerp(HGint32 s0, HGint32 s1, HGint32 factor)
{
    HGint64 v;
    HGint32 result;
    if (hgInRange32(factor, 0, IUNITSCALAR_ONE) == 0)
        imAssert("hgInRange32(factor, 0, IUNITSCALAR_ONE)", "IUnitScalar.c", 36);
    if (hgInRange32(s0, 0, IUNITSCALAR_ONE) == 0)
        imAssert("hgInRange32(s0, 0, IUNITSCALAR_ONE)", "IUnitScalar.c", 37);
    if (hgInRange32(s1, 0, IUNITSCALAR_ONE) == 0)
        imAssert("hgInRange32(s1, 0, IUNITSCALAR_ONE)", "IUnitScalar.c", 38);
    v = hgMul64(IUNITSCALAR_ONE - factor, s0);
    v = hgMadd64(v, factor, s1);
    result = hgLsl64h_0_31(v, 3);
    if (hgInRange32(result, 0, IUNITSCALAR_ONE) == 0)
        imAssert("hgInRange32(result, 0, IUNITSCALAR_ONE)", "IUnitScalar.c", 42);
    return result;
}

HGint32 IUnitScalar_tos(HGint32 x)
{
    /* unit scalar -> IScalar: renormalize the 2.30 value into a float */
    HGint32 a = hgAbs32(x);
    HGint32 sh = hgClz32((HGuint32)a) - 2;
    HGuint32 e;
    if (IUnitScalar_isValid(x) == 0)
        imAssert("IUnitScalar_isValid(x)", "IUnitScalar.c", 62);
    if (x != 0)
        e = (HGuint32)(127 - sh);
    else
        e = 0;
    return IScalar_set(e, x << sh);
}
