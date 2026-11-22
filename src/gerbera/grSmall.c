/* grSmall.c - small core utilities (start of the port; grows as pieces of
 * the original grSmall.c are brought over). */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "hg/hgInt32.h"
#include <stdint.h>

/* grIsAligned - verbatim (docs/libGLES_CM.dll.c:13072-13075, 3 TU copies) */
int grIsAligned(void *ptr, int alignment)
{
    return ((uintptr_t)(alignment - 1) & (uintptr_t)ptr) == 0;
}

/* grIsValidBool - verbatim (docs/libGLES_CM.dll.c:9187-9195, 2 TU copies) */
int grIsValidBool(int v)
{
    if (v == 1)
        return 1;
    return v == 0;
}

/* --- grComputeGradientReference (grSmall.c 153; DLL 25334-25342) ----------- */

int grComputeGradientReference(int v0sx, int v0sy, int xAdd, int yAdd)
{
    HGint64 v4 = hgMul64(v0sx, xAdd);
    HGint64 v5 = hgMadd64(v4, v0sy, yAdd);

    return -hgLsl64h_0_31(v5, 20);
}

/* --- grComputeDepthOffset (grSmall.c 165; DLL 25902-25940) ------------------
 * Adjusts the depth-gradient reference for polygon offset: units scale with
 * the depth-buffer format (byte at drawSurface+10 = depthBytes), factor
 * scales with the steeper of the two depth adders. */
void grComputeDepthOffset(Gerbera_s *state)
{
    _grPixelPipe *pixelPipeline = (_grPixelPipe *)state->pixelPipeline;
    int units = state->depthOffsetUnits;
    int ref = units + pixelPipeline->reference[0];
    int factor = state->depthOffsetFactor;

    switch (((unsigned char *)&state->drawSurface)[10])   /* depthBytes */
    {
        case 1:
            ref += units << 8;
            break;
        case 2:
            ref += units;
            break;
        case 4:
            ref += units >> 16;
            break;
        default:
            break;
    }
    if (factor != 0)
    {
        int xAdder = hgAbs32(pixelPipeline->xAdder[0]);
        int yAdder = hgAbs32(pixelPipeline->yAdder[0]);
        int v1 = hgMax32(xAdder, yAdder);
        HGint64 v2 = hgMul64(factor, v1);

        ref += hgLsl64h(v2, 16);
    }
    pixelPipeline->reference[0] = ref;
}
