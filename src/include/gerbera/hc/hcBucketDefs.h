/* hcBucketDefs.h - per-register definition-site buckets + bitset LRU cache.
 *
 * Layout verified from CodeView LF_STRUCTURE TI=0x1A62 / TI=0x1A65:
 *   +0   int numBuckets
 *   +4   unsigned short *bucketStart
 *   +8   HCBucketDefsCache_s bitVectorCache[16]
 *   +136 unsigned short values[1]
 */
#ifndef GERBERA_HC_BUCKETDEFS_H
#define GERBERA_HC_BUCKETDEFS_H

#include "gerbera/hc/hcBitSet.h"
#include "gerbera/hc/hcMemory.h"
#include "gerbera/hc/hcOpcode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCBucketDefsCache_s
{
    int reg;
    HCBitSet_s *vector;
} HCBucketDefsCache_s;

typedef struct HCBucketDefs_s
{
    int numBuckets;
    unsigned short *bucketStart;
    HCBucketDefsCache_s bitVectorCache[16];
    unsigned short values[1];
} HCBucketDefs_s;

HCBucketDefs_s *HCBucketDefs_create(HCMemory_s *mgr, HCInstr_s *lst,
                                    int nBuckets, int nValues);
int HCBucketDefs_numValues(HCBucketDefs_s *self, int bk);
int HCBucketDefs_getValue(HCBucketDefs_s *self, int bk, int ndxInBucket);
HCBitSet_s *HCBucketDefs_getCachedSet(HCBucketDefs_s *self, int t);
void HCBucketDefs_sub(HCBucketDefs_s *self, HCBitSet_s *set, int t);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_BUCKETDEFS_H */
