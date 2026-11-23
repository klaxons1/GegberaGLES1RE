/* hcStack.h - arena-backed LIFO stack of uint16 indices (hcStack.h/.c).
 *
 * Layout verified from CodeView LF_STRUCTURE TI=0x1AA0 in docs/libGLES_CM.dll:
 *   +0 int allocSize
 *   +4 int numElems
 *   +8 unsigned short elements[2]
 */
#ifndef GERBERA_HC_STACK_H
#define GERBERA_HC_STACK_H

#include "gerbera/hc/hcMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCStack_s
{
    int allocSize;
    int numElems;
    unsigned short elements[2];
} HCStack_s;

HCStack_s *HCStack_create(HCArena_s *mgr);
void HCStack_push(HCArena_s *mgr, HCStack_s *self, int value);
int HCStack_pop(HCStack_s *self);
int HCStack_isEmpty(HCStack_s *self);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_STACK_H */
