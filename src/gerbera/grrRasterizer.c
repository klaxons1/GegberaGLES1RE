/* grrRasterizer.c - clipped primitive entry points into the rasterizer
 * (ported from grrRasterizer.inl).
 */
#include "gerbera/grState.h"

void grrDrawClippedPoint(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    grDrawClippedPoint(state, tri, origIndices);
}

void grrDrawClippedLine(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    grDrawClippedLine(state, tri, origIndices);
}

void grrDrawClippedTriangle(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    if (grTestCapability(state, GR_CAPABILITY_PERSPECTIVE_CORRECTION) != 0)
        grTriangleTessellator(state, tri, origIndices);
    else
        grDrawClippedTriangle(state, tri, origIndices, 8);
}
