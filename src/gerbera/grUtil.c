/* grUtil.c - Gerbera core primitives: assert and die.
 *
 * Ported from the decompilation: grAssert at docs/libGLES_CM.dll.c:7622,
 * grDie at 7741.  The reference build crashed via null dereference; on a
 * hosted build we abort after printing the same message.
 */
// clang-format off
#include "gerbera/grUtil.h"
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "gerbera/grRGBA.h"
#include "hg/hgInt32.h"

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

/* --- matrix dirty helpers (grUtil.h inlines, decompile 24642-24672 and
 * 34783-34798; both grFast/grSmall TU variants share this source) --------- */

int grIsMatrixDirty(Gerbera_s *state, int mode)
{
    if (!(state != NULL) && grAssert("state", "grUtil.h", 167) != 0)
        grDie(0);
    if (!(hgInRange32(mode, 0, GR_MATRIXMODE_MAX) != 0) && grAssert("hgInRange32((int)mode, 0, GR_MATRIXMODE_MAX)", "grUtil.h", 168) != 0)
        grDie(0);
    return ((1 << mode) & state->matrixDirtyMask) != 0;
}

void grSetMatrixDirty(Gerbera_s *state, int mode, int status)
{
    if (!(state != NULL) && grAssert("state", "grUtil.h", 174) != 0)
        grDie(0);
    if (!(hgInRange32(mode, 0, GR_MATRIXMODE_MAX) != 0) && grAssert("hgInRange32((int)mode, 0, GR_MATRIXMODE_MAX)", "grUtil.h", 175) != 0)
        grDie(0);
    /* verbatim bit juggling: clear bit `mode`, then set it from `status`.
     * __ROL4__(-2, mode) == ~(1u << mode) for 0..31. */
    state->matrixDirtyMask = (state->matrixDirtyMask & ~(1u << mode)) | (status << mode);
}

/* --- state-free grUtil.h helpers (decompile 24535-24589, 34837-34877) ----- */

int grVector3iGet(_grVector3i *v, int ndx)
{
    GR_UTIL_CHECK(hgInRange32(ndx, 0, 2), "grUtil.h", 53);
    return *(&v->x + ndx);
}

int grUnpackIndex(unsigned int indices, int n)
{
    GR_UTIL_CHECK(hgInRange32(n, 0, 3), "grUtil.h", 76);
    return (int)(unsigned char)(indices >> (8 * n));
}

unsigned int grSetIndices(int i0, int i1, int i2)
{
    GR_UTIL_CHECK(hgInRange32(i0, 0, 255), "grUtil.h", 82);
    GR_UTIL_CHECK(hgInRange32(i1, 0, 255), "grUtil.h", 83);
    GR_UTIL_CHECK(hgInRange32(i2, 0, 255), "grUtil.h", 84);
    return (unsigned int)i0 | ((unsigned int)i1 << 8) | ((unsigned int)i2 << 16);
}

int grMaskedColorsEqual(unsigned int color, unsigned int mask)
{
    int ndx = 31 - hgClz32(mask);
    int ref = grUnpackRGBA(color, ndx);
    int diff, diffa, diffb;

    GR_UTIL_CHECK((mask & 0xFu) != 0, "grUtil.h", 251);
    GR_UTIL_CHECK((mask & 0xFFFFFFF0u) == 0, "grUtil.h", 252);
    GR_UTIL_CHECK(hgInRange32(ndx, 0, 3), "grUtil.h", 253);
    diff = (ref ^ grUnpackRGBA(color, GR_COLOR_R)) & hgBitMask32((int)mask, 0);
    diffa = ((ref ^ grUnpackRGBA(color, GR_COLOR_G)) & hgBitMask32((int)mask, 1)) | diff;
    diffb = ((ref ^ grUnpackRGBA(color, GR_COLOR_B)) & hgBitMask32((int)mask, 2)) | diffa;
    return (((ref ^ grUnpackRGBA(color, GR_COLOR_A)) & hgBitMask32((int)mask, 3)) | diffb) == 0;
}
