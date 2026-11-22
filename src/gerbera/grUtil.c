/* grUtil.c - Gerbera core primitives: assert and die.
 *
 * Ported from the decompilation: grAssert at docs/libGLES_CM.dll.c:7622,
 * grDie at 7741.  The reference build crashed via null dereference; on a
 * hosted build we abort after printing the same message.
 */
#include "gerbera/grUtil.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void grPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}

int grAssert(const char *expr, const char *file, int line)
{
    grPrintf("\nAssertion: %s (%s %d)\n", expr, file, line);
    return 1;
}

void grDie(int v)
{
    /* the reference binary crashes here (null dereference / Symbian panic);
     * same fail-fast semantics on a hosted build */
    HG_UNUSED(v);
    fflush(stdout);
    fprintf(stderr, "grDie: fatal driver error\n");
    fflush(stderr);
    abort();
}
