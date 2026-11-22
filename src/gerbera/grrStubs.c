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
