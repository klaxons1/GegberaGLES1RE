/* hcAdjMtx.c - lower-triangular undirected adjacency matrix (hcAdjMtx.c/.h).
 *
 * Ported from docs/libGLES_CM.dll.c:42802-42818 and inline helpers in
 * hcAdjMtx.h (HCAdjMtx_calcIndex, HCAdjMtx_connect, HCAdjMtx_hasEdge).
 */
#include "gerbera/hc/hcAdjMtx.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>

int HCAdjMtx_calcIndex(int row, int column)
{
    if (!(row >= 0 && column >= 0))
        HC_assertFail("(row >= 0) && (column >= 0)", "hcAdjMtx.h", 43, NULL);
    return column + ((row * (row - 1)) >> 1);
}

HCAdjMtx_s *HCAdjMtx_create(HCMemory_s *mgr, int numNodes)
{
    HCAdjMtx_s *self;
    int size;

    if (mgr == NULL)
        HC_assertFail("mgr", "hcAdjMtx.c", 39, NULL);
    if (numNodes <= 0)
        HC_assertFail("numNodes > 0", "hcAdjMtx.c", 40, NULL);

    size = HCAdjMtx_calcIndex(numNodes - 1, numNodes - 1) + 1;
    self = (HCAdjMtx_s *)HCMemory_alloc(mgr, (unsigned int)sizeof(HCAdjMtx_s));
    self->numNodes = numNodes;
    self->matrix = HCBitSet_create(mgr, size);
    return self;
}

void HCAdjMtx_connect(HCAdjMtx_s *self, int from, int to)
{
    if (!(self != NULL && self->matrix != NULL))
        HC_assertFail("self && self->matrix", "hcAdjMtx.h", 60, NULL);
    if (!(from >= 0 && from < self->numNodes))
        HC_assertFail("from >= 0 && from < self->numNodes", "hcAdjMtx.h", 61, NULL);
    if (!(to >= 0 && to < self->numNodes))
        HC_assertFail("to >= 0 && to < self->numNodes", "hcAdjMtx.h", 62, NULL);
    if (from < to)
        HC_assertFail("from >= to", "hcAdjMtx.h", 63, NULL);

    HCBitSet_or(self->matrix, HCAdjMtx_calcIndex(from, to));
}

int HCAdjMtx_hasEdge(HCAdjMtx_s *self, int from, int to)
{
    if (!(self != NULL && self->matrix != NULL))
        HC_assertFail("self && self->matrix", "hcAdjMtx.h", 80, NULL);
    if (!(from >= 0 && from < self->numNodes))
        HC_assertFail("from >= 0 && from < self->numNodes", "hcAdjMtx.h", 81, NULL);
    if (!(to >= 0 && to < self->numNodes))
        HC_assertFail("to >= 0 && to < self->numNodes", "hcAdjMtx.h", 82, NULL);
    if (from < to)
        HC_assertFail("from >= to", "hcAdjMtx.h", 83, NULL);

    return HCBitSet_contains(self->matrix, HCAdjMtx_calcIndex(from, to));
}
