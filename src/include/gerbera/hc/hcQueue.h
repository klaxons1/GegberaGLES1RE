/* hcQueue.h - arena-backed circular FIFO queue of uint16 indices (hcQueue.h).
 *
 * 32-bit layout from CodeView LF_STRUCTURE TI=0x1A9B in docs/libGLES_CM.dll:
 *   +0  int head
 *   +4  int tail
 *   +8  int allocSize
 *   +12 int nElems
 *   +16 unsigned short *elements
 */
#ifndef GERBERA_HC_QUEUE_H
#define GERBERA_HC_QUEUE_H

#include "gerbera/hc/hcMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCQueue_s
{
    int head;
    int tail;
    int allocSize;
    int nElems;
    unsigned short *elements;
} HCQueue_s;

HCQueue_s *HCQueue_create(HCArena_s *mgr);
int HCQueue_isEmpty(HCQueue_s *self);
void HCQueue_enqueue(HCArena_s *mgr, HCQueue_s *self, int value);
int HCQueue_dequeue(HCQueue_s *self);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_QUEUE_H */
