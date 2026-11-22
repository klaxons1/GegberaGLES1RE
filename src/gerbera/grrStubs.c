/* grrStubs.c - scaffolding for the rasterizer-side entry points that
 * grFlushPrimitiveBatch and grClipPrimitive reach.
 *
 * grrDrawClippedTriangle, grInterpolateVertexGradients, grDrawClippedLine
 * and grDrawClippedPoint are now REAL ports. Still NOT a port:
 * grTriangleTessellator (tessellator session). grUpdateVertexPipeline is
 * already ported (grVertexPipeline.c).
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

/* grTriangleTessellator (grTessellator.inl 557; DLL 27249-27359) is NOT
 * ported yet: perspective-correct subdivision (grClassifyTriangle/
 * grSplitEdge/grTessellateTriangle). Silent stub until the tessellator
 * session; tests keep GR_CAPABILITY_PERSPECTIVE_CORRECTION clear. */
void grTriangleTessellator(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    (void)state;
    (void)tri;
    (void)origIndices;
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
