/* grUtil.h - Gerbera core primitives: assert and die.
 *
 * The original assert machinery (recovered from the decompilation):
 *   grAssert(expr, file, line)  - prints "\nAssertion: %s (%s %d)\n",
 *                                 returns 1 (failure marker)
 *   grDie(v)                    - fatal handler; the reference build crashes
 *                                 by dereferencing null (Symbian exc panic)
 *
 * Call sites in the ported core follow the decompiled shape:
 *   ok = grAssert("expr", "file.inl", line) == 0;
 *   if (!ok) grDie(0);
 */
#ifndef GR_UTIL_H
#define GR_UTIL_H

#include "hg/hgDefs.h"

/* grDefs.h defines _grVector3i; pointer-only here to avoid an include cycle */
struct _grVector3i;

HG_EXTERN_C_BEGIN

void grPrintf(const char *format, ...);
int  grAssert(const char *expr, const char *file, int line);
void grDie(int v) ; /* marked noreturn in the port */

/* --- state-free grUtil.h helpers (need grDefs.h types + grRGBA.h) -------- */
int grVector3iGet(struct _grVector3i *v, int ndx);
int grUnpackIndex(unsigned int indices, int n);
unsigned int grSetIndices(int i0, int i1, int i2);
int grMaskedColorsEqual(unsigned int color, unsigned int mask);

/* combined check used by the ported .inl code */
#define GR_CHECK(cond, file, line)                                          \
    do {                                                                    \
        HGbool gr_ok_ = HG_TRUE;                                            \
        if (!(cond))                                                        \
            gr_ok_ = grAssert(#cond, (file), (line)) == 0;                  \
        if (!gr_ok_)                                                        \
            grDie(0);                                                       \
    } while (0)

HG_EXTERN_C_END

#define GR_UTIL_CHECK(cond, file, line)          \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, (file), (line)) != 0) \
            grDie(0);                            \
    } while (0)

#endif /* GR_UTIL_H */
