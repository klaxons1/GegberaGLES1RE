/* hgMemory.h - memory wrappers (tail of hgInt32.inl / hgMemorySymbian.cpp).
 *
 * The reference build routed these to Symbian's fast Mem::Fill/Copy/Move/
 * Compare primitives.  On hosted platforms the standard C library provides
 * the same semantics.
 */
#ifndef HG_MEMORY_H
#define HG_MEMORY_H

#include "hg/hgDefs.h"

#include <string.h>

HG_EXTERN_C_BEGIN

#define hgMemSet(d, v, n)    memset((d), (v), (n))
#define hgMemCopy(d, s, n)   memcpy((d), (s), (n))
#define hgMemMove(d, s, n)   memmove((d), (s), (n))
#define hgMemCompare(a, b, n) memcmp((a), (b), (n))

HG_EXTERN_C_END

#endif /* HG_MEMORY_H */
