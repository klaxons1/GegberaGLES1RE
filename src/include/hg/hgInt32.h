/* hgInt32.h - 32/64 bit integer helpers (hgInt32.inl, hgInt64Native.inl).
 *
 * The 64-bit helpers use C integer operations for the hgInt64Native.inl API.
 */
#ifndef HG_INT32_H
#define HG_INT32_H

#include "hg/hgDefs.h"

HG_EXTERN_C_BEGIN

/* --- 32 bit helpers ------------------------------------------------ */

int          hgClz32(HGuint32 value);          /* count leading zeros; returns 32 for zero */
int          hgInRange32(int a, int lo, int hi);
int          hgMin32(int a, int b);
int          hgMax32(int a, int b);
HGuint32     hgMinu32(HGuint32 a, HGuint32 b);
HGuint32     hgMaxu32(HGuint32 a, HGuint32 b);
int          hgAbs32(int a);
int          hgSign32(int value);
int          hgSignMask32(int x);              /* x >> 31 (all sign bits)      */
int          hgSignBit32(int x);               /* x >> 31 unsigned (0 or 1)    */
int          hgMaxZero32(int a);               /* max(a, 0) without branches   */
int          hgClamp32(int v, int mn, int mx);
int          hgClamp32_0_x(int v, int mx);
int          hgIsPowerOfTwo32(HGuint32 x);
int          hgDiff32(int a, int b);           /* abs(a - b)                   */
int          hgRor32(int a, int sh);           /* rotate right, sh in [0,32]   */
int          hgCopySign32(int d, int s);       /* s < 0 ? -d : d, requires d>=0 */
int          hgBitMask32(int x, int i);        /* bit i expanded to 0 or -1 */
int          hgAdds32(int a, int b);           /* saturating signed add        */

/* --- float bit conversions (hgInt32.inl) --------------------------- */

HGuint32     hgFloatBitPattern(HGfloat f);
HGfloat      hgBitPatternToFloat(HGuint32 x);

/* --- endianess ------------------------------------------------------ */

HGbool       hgGetEndianess(void);             /* 1 for big endian, 0 for little endian */

/* --- 64 bit helpers (hgInt64Native.inl) ----------------------------- */

HGint64      hgToSigned64(HGuint64 a);
HGuint64     hgToUnsigned64(HGint64 a);

HGint64      hgSet64s(int l);
HGuint64     hgSet64u(HGuint32 h, HGuint32 l);
HGint64      hgSet64(int h, int l);
HGint64      hgSet64x(int hi);                 /* hi << 32                     */
HGint64      hgZero64(void);
int          hgGet64h(HGint64 a);
HGuint32     hgGet64uh(HGuint64 a);
HGuint32     hgGet64ul(HGuint64 a);
int          hgGet64l(HGint64 a);

int          hgGetBit64(HGint64 x, int offset);
HGint64      hgSetBit64(HGint64 x, int offset);

HGint64      hgAdd64(HGint64 a, HGint64 b);
HGint64      hgSub64(HGint64 a, HGint64 b);
HGint64      hgOr64(HGint64 a, HGint64 b);
int          hgLessThan64(HGint64 a, HGint64 b);
int          hgLessThan64u(HGuint64 a, HGuint64 b);
int          hgLessThan64_32u(HGuint64 a, HGuint32 b);
int          hgIsZero64(HGint64 a);
int          hgLessThanZero64(HGint64 a);
int          hgEq64(HGint64 a, HGint64 b);

HGint64      hgLsl64(HGint64 a, int sh);
int          hgLsl64h(HGint64 a, int sh);      /* (a << sh) >> 32              */
int          hgLsl64h_0_31(HGint64 a, int sh); /* pre-conditioned variant      */
HGint64      hgLsr64(HGint64 a, int sh);
HGint64      hgAsr64(HGint64 a, int sh);
HGint64      hgAsr64_0_31(HGint64 a, int sh);

HGint64      hgNeg64(HGint64 a);
HGint64      hgNot64(HGint64 a);
HGint64      hgAbs64(HGint64 a);

HGint64      hgMul64(int a, int b);            /* (HGint64)a * b               */
HGuint64     hgMulu64(HGuint32 a, HGuint32 b);
int          hgMul64h(int a, int b);           /* high 32 bits of signed mul   */
HGuint32     hgMulu64h(HGuint32 a, HGuint32 b);/* high 32 bits of unsigned mul */
HGint64      hgMul64x32(HGint64 a, int b);
HGint64      hgMadd64(HGint64 a, int b, int c);/* a + (HGint64)b * c           */
int          hgMadd64h(HGint64 a, int b, int c);
int          hgClz64(HGint64 a);

HGuint32     hgDiv32Internal(HGuint32 *cio, HGuint64 *aio, HGuint32 b, int n);
HGuint64     hgDivu64_32(HGuint64 a, HGuint32 b);
HGint64      hgDiv64_32(HGint64 a, int b);

HG_EXTERN_C_END

#endif /* HG_INT32_H */
