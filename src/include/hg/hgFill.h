/* hgFill.h - fast fill routines (hgFill.c).
 *
 * The driver fills render targets and buffers with these instead of memset
 * so that any 8/16/32 bit pattern can be replicated.
 */
#ifndef HG_FILL_H
#define HG_FILL_H

#include "hg/hgDefs.h"

HG_EXTERN_C_BEGIN

void hgFill8(HGuint8 *d, HGuint8 v, int N);
void hgFill16(HGuint16 *d, HGuint16 v, int N);
void hgFill32(HGuint32 *d, HGuint32 v, int N);

/* internal: N must be a multiple of 8 or the tail is left unfilled */
void hgFill32_inner(HGuint32 *d, HGuint32 v, int N);

HG_EXTERN_C_END

#endif /* HG_FILL_H */
