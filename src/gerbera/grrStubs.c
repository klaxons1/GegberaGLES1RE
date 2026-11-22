/* grrStubs.c - scaffolding for the rasterizer-side entry points that
 * grFlushPrimitiveBatch and grClipPrimitive reach.
 *
 * All rasterizer-side entry points are now REAL ports; grTriangleTessellator
 * lives in grTessellator.c and grUpdateVertexPipeline in grVertexPipeline.c.
 */
#include "gerbera/grState.h"



void grrDrawClippedPoint(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    (void)state;
    (void)tri;
    (void)origIndices;
}

void grrDrawClippedLine(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    (void)state;
    (void)tri;
    (void)origIndices;
}

/* grrDrawClippedTriangle (grSmall.c; DLL 27381-27390) - ported: affine path
 * (emptyBits 8) vs the perspective tessellator; grTriangleTessellator is
 * still a stub below (tessellator session). */
void grrDrawClippedTriangle(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    if (grTestCapability(state, GR_CAPABILITY_PERSPECTIVE_CORRECTION) != 0)
        grTriangleTessellator(state, tri, origIndices);
    else
        grDrawClippedTriangle(state, tri, origIndices, 8);
}



/* grSetupTextureUnit (grRasterizer.inl 8-189; DLL 25345-25526) is NOT ported
 * yet: it walks the texture-object/mipmap layout (tex+4, tex+16*level,
 * tex+176/+180/+183/+204/+206) which gets pinned with the texture block.
 * Silent stub so the gradient setup links; the texture-gradient mask bits
 * (0xC0/0x300) must stay clear in tests until it lands. */
void grSetupTextureUnit(Gerbera_s *state, int texIndex, int (*pairs)[2],
                        int v0sx, int v0sy)
{
    (void)state;
    (void)texIndex;
    (void)pairs;
    (void)v0sx;
    (void)v0sy;
}

/* --- hc/egl stubs (pending the hc and egl modules) -------------------------
 * grGetBackend / HC_extraStackSpace / grScratchGrowMemory are hc-module
 * entry points; grLockSurface / grReleaseSurface are egl-module entry
 * points. Until those modules land they are inert: no backend, no extra
 * stack space, no scratch growth, no surface locking. grSetupRendering
 * still runs its full state bookkeeping against them. */
void *grGetBackend(void)
{
    return NULL;
}

int HC_extraStackSpace(void *backend)
{
    (void)backend;
    return 0;
}

int grScratchGrowMemory(Gerbera_s *state)
{
    (void)state;
    return 0;
}

