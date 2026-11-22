/* grrStubs.c - die-stubs for the rasterizer-side entry points that
 * grFlushPrimitiveBatch and grClipPrimitive reach.
 *
 * These are SCAFFOLDING, not ports: the real implementations land with the
 * rasterizer session (grrDrawClippedPoint/Line/Triangle + grInterpolate-
 * VertexGradients) and the vertex-pipeline stage-selector session
. grUpdateVertexPipeline
 * is already ported (grVertexPipeline.c). Tests never reach them (grDie
 * aborts).
 */
#include "gerbera/grState.h"

void grInterpolateVertexGradients(Gerbera_s *state, _grVertex *vertex,
                                  unsigned int origIndices, int side)
{
    (void)state;
    (void)vertex;
    (void)origIndices;
    (void)side;
    grDie(0);
}

void grrDrawClippedPoint(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    (void)state;
    (void)tri;
    (void)origIndices;
    grDie(0);
}

void grrDrawClippedLine(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    (void)state;
    (void)tri;
    (void)origIndices;
    grDie(0);
}

void grrDrawClippedTriangle(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    (void)state;
    (void)tri;
    (void)origIndices;
    grDie(0);
}
