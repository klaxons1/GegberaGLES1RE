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

/* IMath.c also defines IVector3_isValid / IVector4_isValid; they move in
 * with the IVector port (they need _IVector3/_IVector4). */
