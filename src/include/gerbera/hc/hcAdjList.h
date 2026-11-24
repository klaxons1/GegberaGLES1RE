/* hcAdjList.h - compressed interference-graph adjacency list (hcAdjList.h).
 *
 * 32-bit layout from CodeView LF_STRUCTURE TI=0x1996 in docs/libGLES_CM.dll:
 *   +0 int numNodes
 *   +4 unsigned short *lowerLists[1]   (numNodes row pointers)
 */
#ifndef GERBERA_HC_ADJLIST_H
#define GERBERA_HC_ADJLIST_H

#include "gerbera/hc/hcAdjMtx.h"
#include "gerbera/hc/hcMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCAdjList_s
{
    int numNodes;
    unsigned short *lowerLists[1];
} HCAdjList_s;

HCAdjList_s *HCAdjList_create(HCMemory_s *mgr, HCAdjMtx_s *mtx);
int HCAdjList_getEdges(HCAdjList_s *self, int from, unsigned short *to);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_ADJLIST_H */
