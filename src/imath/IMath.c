/* IMath.c - validity checkers shared by the imath types.
 *
 * Clean port of the decompiled IMath.c scalar/vector validity functions.
 * The vector functions require the IVector3/IVector4 types (ported with the
 * vector layer); only the scalar pair is compiled here.
 */
#include "imath/imApi.h"
#include "hg/hgInt32.h"
#include "gerbera/grUtil.h"

HGint32 IScalar_isValid(HGint32 v)
{
    if (IScalar_getExponent(v) != 0)
    {
        HGint32 mantissa;
        if (IScalar_getExponent(v) > 0xFE)
            return 0;
        mantissa = IScalar_getMantissa(v);
        if ((hgAbs32(mantissa) & IUNITSCALAR_ONE) == 0)
            imAssert("bsum & IUNITSCALAR_ONE", "IMath.c", 103);
        mantissa = IScalar_getMantissa(v);
        if (hgInRange32(mantissa, -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(IScalar_getMantissa(v), -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 104);
        return 1;
    }
    else
    {
        if (IScalar_getMantissa(v) != 0)
            imAssert("IScalar_getMantissa(v) == 0", "IMath.c", 97);
        return 1;
    }
}

HGint32 IUnitScalar_isValid(HGint32 v)
{
    if (hgInRange32(v, -IUNITSCALAR_ONE, IUNITSCALAR_ONE) == 0)
        imAssert("hgInRange32(v, -IUNITSCALAR_ONE, IUNITSCALAR_ONE)", "IMath.c", 116);
    return 1;
}

HGint32 IVector3_isValid(_IVector3 *v)
{
    if (v->e != 0)
    {
        int bsum;
        if (v->e > 0xFEu)
            return 0;
        bsum = hgAbs32(v->m[0]) | hgAbs32(v->m[1]) | hgAbs32(v->m[2]);
        if ((bsum & IUNITSCALAR_ONE) == 0)
            imAssert("bsum & IUNITSCALAR_ONE", "IMath.c", 0x84u);
        if (hgInRange32(v->m[0], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[0], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0x85u);
        if (hgInRange32(v->m[1], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[1], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0x86u);
        if (hgInRange32(v->m[2], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[2], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0x87u);
        return 1;
    }
    else
    {
        if (v->m[0] != 0)
            imAssert("v->m[0] == 0", "IMath.c", 0x7Cu);
        if (v->m[1] != 0)
            imAssert("v->m[1] == 0", "IMath.c", 0x7Du);
        if (v->m[2] != 0)
            imAssert("v->m[2] == 0", "IMath.c", 0x7Eu);
        return 1;
    }
}

HGint32 IVector4_isValid(_IVector4 *v)
{
    if (v->e != 0)
    {
        int bsum;
        if (v->e > 0xFEu)
            return 0;
        bsum = hgAbs32(v->m[0]) | hgAbs32(v->m[1]) | hgAbs32(v->m[2]) | hgAbs32(v->m[3]);
        if ((bsum & IUNITSCALAR_ONE) == 0)
            imAssert("bsum & IUNITSCALAR_ONE", "IMath.c", 0x9Eu);
        if (hgInRange32(v->m[0], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[0], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0x9Fu);
        if (hgInRange32(v->m[1], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[1], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0xA0u);
        if (hgInRange32(v->m[2], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[2], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0xA1u);
        if (hgInRange32(v->m[3], -IM_MANTISSA_MAX, IM_MANTISSA_MAX) == 0)
            imAssert("hgInRange32(v->m[3], -IM_MANTISSA_MAX, IM_MANTISSA_MAX)", "IMath.c", 0xA2u);
        return 1;
    }
    else
    {
        if (v->m[0] != 0)
            imAssert("v->m[0] == 0", "IMath.c", 0x95u);
        if (v->m[1] != 0)
            imAssert("v->m[1] == 0", "IMath.c", 0x96u);
        if (v->m[2] != 0)
            imAssert("v->m[2] == 0", "IMath.c", 0x97u);
        if (v->m[3] != 0)
            imAssert("v->m[3] == 0", "IMath.c", 0x98u);
        return 1;
    }
}

/* IMath.c also defines nothing else; the matrix layer lives in IMatrix4.c. */
