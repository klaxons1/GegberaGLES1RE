/* hcBitSet.h - arena-allocated dense bitset (hcBitSet.h of the original).
 *
 * 32-bit layout from CodeView LF_STRUCTURE TI=0x1989 in docs/libGLES_CM.dll:
 *   +0 int length
 *   +4 unsigned int data[1]   (ceil(length / 32) 32-bit words)
 */
#ifndef GERBERA_HC_BITSET_H
#define GERBERA_HC_BITSET_H

#include "gerbera/hc/hcMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCBitSet_s
{
    int length;
    unsigned int data[1];
} HCBitSet_s;

void HCBitSet_selfCheckInternal(HCBitSet_s *self);
void HCBitSet_selfCheck(HCBitSet_s *self);

HCBitSet_s *HCBitSet_createNoClear(HCMemory_s *mgr, int length);
HCBitSet_s *HCBitSet_create(HCMemory_s *mgr, int length);

int HCBitSet_contains(HCBitSet_s *self, int v);
void HCBitSet_or(HCBitSet_s *self, int v);
void HCBitSet_sub(HCBitSet_s *self, int v);

void HCBitSet_copy(HCBitSet_s *dst, HCBitSet_s *src);
void HCBitSet_clear(HCBitSet_s *self);
void HCBitSet_set(HCBitSet_s *self);
int HCBitSet_equal(HCBitSet_s *a, HCBitSet_s *b);

int HCBitSet_numElements(HCBitSet_s *self);
int HCBitSet_getFirst(HCBitSet_s *self);
int HCBitSet_getNext(HCBitSet_s *self, int first);
int HCBitSet_getFirstUnset(HCBitSet_s *self);
int HCBitSet_getNextUnset(HCBitSet_s *self, int first);

void HCBitSet_union(HCBitSet_s *dst, HCBitSet_s *src);
void HCBitSet_intersect(HCBitSet_s *dst, HCBitSet_s *src);
void HCBitSet_difference(HCBitSet_s *dst, HCBitSet_s *src);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_BITSET_H */
