/* grRGBA.h - RGBA8888 packing helpers (port of the decompiled grRGBA.h).
 *
 * Color word layout: (a << 24) | (b << 16) | (g << 8) | r.
 * GR_COLOR_MAX is 4 (the channel loop runs i < GR_COLOR_MAX, i.e. R..A);
 * the decompiled range asserts use hgInRange32(ndx, 0, GR_COLOR_MAX),
 * which is inclusive - ndx == GR_COLOR_MAX passes them (faithful quirk).
 */
#ifndef GR_RGBA_H
#define GR_RGBA_H

#include "hg/hgDefs.h"
#include "gerbera/grUtil.h"

HG_EXTERN_C_BEGIN

enum GR_COLOR_e
{
    GR_COLOR_R = 0,
    GR_COLOR_G = 1,
    GR_COLOR_B = 2,
    GR_COLOR_A = 3,
    GR_COLOR_MAX = 4
};

unsigned int grSetRGBA(int r, int g, int b, int a);
int grUnpackRGBA(unsigned int color, int ndx);
unsigned int grPackRGBA(int component, int ndx);

HG_EXTERN_C_END

#define GR_RGBA_CHECK(cond, file, line)          \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, (file), (line)) != 0) \
            grDie(0);                            \
    } while (0)

#endif /* GR_RGBA_H */
