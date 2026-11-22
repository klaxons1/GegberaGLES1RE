/* hgAssert.c - assert failure handler for the hg support library.
 *
 * The reference binary terminated through Symbian's __ASSERT_DEBUG path
 * (PANIC / grDie).  On a hosted build we print the original assert site and
 * abort, which keeps the same fail-fast semantics.
 */
#include "hg/hgDefs.h"

#include <stdio.h>
#include <stdlib.h>

void __assert(const char *file, int line, const char *expr)
{
    fflush(stdout);
    fprintf(stderr, "HG ASSERT FAILED: %s, line %d: %s\n", file, line, expr);
    fflush(stderr);
    abort();
}
