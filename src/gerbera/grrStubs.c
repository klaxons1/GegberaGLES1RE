/* grrStubs.c - scaffolding for the rasterizer-side entry points that
 * grFlushPrimitiveBatch and grClipPrimitive reach.
 *
 * All rasterizer-side entry points are now REAL ports; grTriangleTessellator
 * lives in grTessellator.c and grUpdateVertexPipeline in grVertexPipeline.c.
 */
#include <stdlib.h>
#include "gerbera/grState.h"
#include "gerbera/hc/hcBackend.h"



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

/* --- hc/egl integration & stubs -------------------------------------------
 * grGetBackend (grSymbian.cpp; DLL 24751-24754) returns the active JIT
 * backend (x86 WINSCW reference backend). grScratchGrowMemory awaits the
 * egl allocator (eglAlloc/eglFree). */
void *grGetBackend(void)
{
    return HCX86Backend_create();
}

int grScratchGrowMemory(Gerbera_s *state)
{
    (void)state;
    return 0;
}

void *eglAlloc(unsigned int size)
{
    if (size == 0 && grAssert("size > 0", "eglSymbian.inl", 252) != 0)
        grDie(0);
    return malloc((size_t)size);
}

void eglFree(void *ptr)
{
    if (ptr != NULL)
        free(ptr);
}

