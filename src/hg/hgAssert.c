/* hgAssert.c - assert failure handler for the hg support library.
 *
 * The reference binary terminated through Symbian's __ASSERT_DEBUG path
 * (PANIC / grDie).  On a hosted build we print the original assert site and
 * abort, which keeps the same fail-fast semantics.
 */
#include "hg/hgDefs.h"

#ifdef __SYMBIAN32__
#include "symbian_os.h"

void __assert(const char *file, int line, const char *expr)
{
    HG_UNUSED(file);
    HG_UNUSED(line);
    HG_UNUSED(expr);
    sym_User_Invariant();
}
#else
#include <stdio.h>
#include <stdlib.h>

void __assert(const char *file, int line, const char *expr)
{
    fflush(stdout);
    fprintf(stderr, "HG ASSERT FAILED: %s, line %d: %s\n", file, line, expr);
    fflush(stderr);
    abort();
}
#endif
