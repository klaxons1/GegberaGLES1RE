/* imApi.h - fixed point / bit-pattern scalar math library ("imath").
 *
 * Ported from the Gerbera GLES1 driver, original tree
 * renderer/libs/imath/ (imApi.h, imApi.inl, IScalar.c, IUnitScalar.c,
 * IMath.c).
 *
 * The scalar types are IEEE single precision floats carried around in their
 * HGint32 bit pattern:
 *   - IScalar    arbitrary float (exponent 0..254, mantissa in [-2^30, 2^30))
 *   - IUnitScalar float restricted to [-1, 1) (values [-2^30, 2^30))
 */
#ifndef IM_API_H
#define IM_API_H

#include "hg/hgDefs.h"
#include "hg/hgFloat.h"

HG_EXTERN_C_BEGIN

/* unit scalar domain: 2.30 fixed point */
#define IUNITSCALAR_ONE 0x20000000
#define IM_MANTISSA_MAX 0x3FFFFFFF

typedef HGint32 IScalar;
typedef HGint32 IUnitScalar;

/* imath assert: same semantics as in the reference build - prints through
 * grAssert and dies.  Call sites keep the original file/line strings. */
void imAssert(const char *expr, const char *file, unsigned int line);

/* --- construction / conversion (IScalar.c) -------------------------- */

HGint32  IScalar_clampUnit(HGint32 a);          /* clamp float to [0, 1]   */
HGint32  IScalar_intToScalar(HGint32 a);        /* F_ITOF                  */
HGint32  IScalar_iftos(HGint32 a);              /* float bits -> IScalar   */
HGint32  IScalar_ftos(HGfloat a);
HGfloat  IScalar_stof(HGint32 a);
HGint32  IScalar_immediate(HGfloat a);
HGint32  IScalar_one(void);
HGint32  IScalar_zero(void);
HGint32  IScalar_set(HGuint32 e, HGint32 m);    /* exponent + 6.26 mantissa */
HGint32  IScalar_setAndNormalize(HGuint32 e, HGint32 m);
HGuint32 IScalar_getExponent(HGint32 x);
HGint32  IScalar_getMantissa(HGint32 x);        /* 6.26 signed mantissa    */
HGint32  IScalar_getOESMantissa(HGint32 s);
HGint32  IScalar_getOESExponent(HGint32 s);
HGuint32 IScalar_getUnsignedMantissa(HGint32 x);
HGint32  IScalar_toUnitScalar(HGint32 x);
HGint32  IScalar_fixedToScalar(HGint32 m, HGint32 n);  /* n.fraction fixed */
HGint32  IScalar_scalarToFixed(HGint32 r, HGint32 n);
HGint32  IScalar_byteToScalar(HGuint8 b);       /* b / 255                 */
HGint32  IScalar_toFixedHO(HGint32 value, HGint32 bits);
HGint32  IScalar_toXScalar(HGint32 v);          /* 21 bit fixed            */

/* --- arithmetic (bit pattern ops) ----------------------------------- */

HGint32  IScalar_mul(HGint32 a, HGint32 b);
HGint32  IScalar_add(HGint32 a, HGint32 b);
HGint32  IScalar_sub(HGint32 a, HGint32 b);
HGint32  IScalar_sqr(HGint32 a);
HGint32  IScalar_rcp(HGint32 a);
HGint32  IScalar_rsq(HGint32 a);
HGint32  IScalar_negate(HGint32 x);
HGint32  IScalar_abs(HGint32 x);
HGint32  IScalar_max(HGint32 a, HGint32 b);
HGint32  IScalar_degreesToRadians(HGint32 x);

/* --- compares -------------------------------------------------------- */

HGint32  IScalar_cmpeq(HGint32 a, HGint32 b);
HGint32  IScalar_cmpne(HGint32 a, HGint32 b);
HGint32  IScalar_cmplt(HGint32 a, HGint32 b);
HGint32  IScalar_cmpgt(HGint32 a, HGint32 b);
HGint32  IScalar_cmple(HGint32 a, HGint32 b);
HGint32  IScalar_cmpge(HGint32 a, HGint32 b);
HGint32  IScalar_testeq(HGint32 a);             /* compare against zero    */
HGint32  IScalar_testne(HGint32 a);
HGint32  IScalar_testlt(HGint32 a);
HGint32  IScalar_testgt(HGint32 a);
HGint32  IScalar_testle(HGint32 a);
HGint32  IScalar_testge(HGint32 a);

/* --- misc ------------------------------------------------------------ */

HGuint32 IScalar_log2_iterate(HGuint32 y);      /* log2 in 2.10-ish output */
HGint32  XScalar_log2(HGint32 y);               /* 11.21 log2 or INT_MIN   */
HGint32  IScalar_cos(HGint32 x);

/* NOTE (not yet ported - need _IFVector3 / XScalar layers):
 *   IScalar_expe, IScalar_attenuateRsq, IScalar_attenuateRcp */

/* --- unit scalar domain (IUnitScalar.c) ------------------------------ */

HGint32  IUnitScalar_maxZero(HGint32 x);
HGint32  IUnitScalar_lerp(HGint32 s0, HGint32 s1, HGint32 factor);
HGint32  IUnitScalar_tos(HGint32 x);            /* unit scalar -> IScalar  */
/* IUnitScalar_sin / IUnitScalar_cos live in hg/hgFloat.c (assert provenance) */

/* --- validity (IMath.c) ---------------------------------------------- */

HGint32  IScalar_isValid(HGint32 v);
HGint32  IUnitScalar_isValid(HGint32 v);

HG_EXTERN_C_END

#endif /* IM_API_H */
