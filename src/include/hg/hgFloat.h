/* hgFloat.h - IEEE 754 bit-pattern float helpers (hgFloat.h / hgFloat.inl).
 *
 * Arithmetic uses IEEE single-precision bit patterns; predicates return
 * integer truth values. F_ITOF converts a signed integer to float bits.
 */
#ifndef HG_FLOAT_H
#define HG_FLOAT_H

#include "hg/hgDefs.h"

HG_EXTERN_C_BEGIN

/* float <-> bit pattern */
#define FIN(x)   hgFloatBitPattern(x)
#define FOUT(x)  hgBitPatternToFloat(x)

/* F_ISVALID rejects NaNs; F_FINITE rejects both NaNs and infinities. */
HGint32  F_ISVALID(HGuint32 a);
HGint32  F_FINITE(HGuint32 a);

/* Driver bit ordering; signed zeros compare equal. */
HGint32  F_MAKE_COMPARABLE(HGuint32 a);
HGint32  F_CMPLE(HGuint32 a, HGuint32 b);
HGint32  F_CMPGE(HGuint32 a, HGuint32 b);
HGint32  F_CMPLT(HGuint32 a, HGuint32 b);
HGint32  F_CMPGT(HGuint32 a, HGuint32 b);
HGint32  F_CMPEQ(HGuint32 a, HGuint32 b);
HGint32  F_CMPNE(HGuint32 a, HGuint32 b);

/* compare against zero (sign included) */
HGint32  F_ZCMPEQ(HGuint32 a);
HGint32  F_ZCMPNE(HGuint32 a);
HGint32  F_ZCMPLT(HGuint32 a);
HGint32  F_ZCMPLE(HGuint32 a);
HGint32  F_ZCMPGE(HGuint32 a);
HGint32  F_ZCMPGT(HGuint32 a);

/* arithmetic on bit patterns */
HGuint32 F_ABS(HGuint32 a);
HGuint32 F_NEG(HGuint32 a);
HGuint32 F_ADD(HGuint32 a, HGuint32 b);
HGuint32 F_SUB(HGuint32 a, HGuint32 b);
HGuint32 F_MUL(HGuint32 a, HGuint32 b);
HGuint32 F_SQR(HGuint32 a);
HGuint32 F_ITOF(HGint32 x);
HGuint32 F_RCP(HGuint32 a);
HGuint32 F_RSQ(HGuint32 a);
HGuint32 F_MIN(HGuint32 a, HGuint32 b);
HGuint32 F_MAX(HGuint32 a, HGuint32 b);
HGuint32 F_MAXZERO(HGuint32 a);


/* float building blocks (hgFloat.c) */
HGuint32 extractMantissa(HGuint32 a);
HGint32  extractBiasedExponent(HGuint32 a);
HGuint32 lsrSigned(HGuint32 x, int shift);
HGuint32 buildFloat(HGuint32 signMask, int exponent, HGuint32 mantissa);
HGuint32 buildFloatUnchecked(HGuint32 signMask, int exponent, HGuint32 mantissa);

/* unit scalar trig used by hgFsin/hgFcos.
 * NOTE: IUnitScalar_sin/cos assert into hgFloat.c in the reference binary,
 * so they are ported here rather than into imath. */
HGint32  IUnitScalar_sin(int x);
HGint32  IUnitScalar_cos(int x);
HGint32  radiansToUnitScalar(HGuint32 a);
HGuint32 unitScalarToFloat(int s);

HGuint32 F_SIN(HGuint32 a);
HGuint32 F_COS(HGuint32 a);

HGfloat  hgFsin(HGfloat a);
HGfloat  hgFcos(HGfloat a);
HG_EXTERN_C_END

#endif /* HG_FLOAT_H */
