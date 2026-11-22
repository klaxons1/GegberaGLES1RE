/* grrStubs.c - scaffolding for the rasterizer-side entry points that
 * grFlushPrimitiveBatch and grClipPrimitive reach.
 *
 * These are NOT ports: the real implementations land with the rasterizer
 * session (grrDrawClippedPoint/Line/Triangle + grInterpolateVertex-
 * Gradients). grUpdateVertexPipeline is already ported (grVertexPipeline.c).
 * They return silently so the queue/flush machinery stays testable.
 */
#include "gerbera/grState.h"

void grInterpolateVertexGradients(Gerbera_s *state, _grVertex *vertex,
                                  unsigned int origIndices, int side)
{
    (void)state;
    (void)vertex;
    (void)origIndices;
    (void)side;
}

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

void grrDrawClippedTriangle(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
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
