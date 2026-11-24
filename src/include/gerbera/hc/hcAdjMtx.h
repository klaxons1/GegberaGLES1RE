/* hcAdjMtx.h - lower-triangular undirected adjacency matrix (hcAdjMtx.h).
 *
 * 32-bit layout from CodeView LF_STRUCTURE TI=0x1990 in docs/libGLES_CM.dll:
 *   +0 int numNodes
 *   +4 HCBitSet_s *matrix
 */
#ifndef GERBERA_HC_ADJMTX_H
#define GERBERA_HC_ADJMTX_H

#include "gerbera/hc/hcBitSet.h"
#include "gerbera/hc/hcMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCAdjMtx_s
{
    int numNodes;
    HCBitSet_s *matrix;
} HCAdjMtx_s;

int HCAdjMtx_calcIndex(int row, int column);
HCAdjMtx_s *HCAdjMtx_create(HCMemory_s *mgr, int numNodes);
void HCAdjMtx_connect(HCAdjMtx_s *self, int from, int to);
int HCAdjMtx_hasEdge(HCAdjMtx_s *self, int from, int to);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_ADJMTX_H */
