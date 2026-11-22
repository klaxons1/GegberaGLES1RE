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

HG_EXTERN_C_BEGIN

void grPrintf(const char *format, ...);
int  grAssert(const char *expr, const char *file, int line);
void grDie(int v) ; /* marked noreturn in the port */

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

#endif /* GR_UTIL_H */
