/* hcBucketDefs.c - per-register definition-site buckets (hcBucketDefs.c/.h).
 *
 * Ported from docs/libGLES_CM.dll.c:45808-45972.
 */
#include "gerbera/hc/hcBucketDefs.h"

#include <stddef.h>
#include <string.h>

int HCBucketDefs_numValues(HCBucketDefs_s *self, int bk)
{
    if (self == NULL)
        HC_assertFail("self", "hcBucketDefs.h", 59, NULL);
    if (!(bk >= 0 && bk < self->numBuckets))
        HC_assertFail("bk >= 0 && bk < self->numBuckets", "hcBucketDefs.h", 60, NULL);
    return self->bucketStart[bk + 1] - self->bucketStart[bk];
}

int HCBucketDefs_getValue(HCBucketDefs_s *self, int bk, int ndxInBucket)
{
    if (self == NULL)
        HC_assertFail("self", "hcBucketDefs.h", 66, NULL);
    if (!(bk >= 0 && bk < self->numBuckets))
        HC_assertFail("bk >= 0 && bk < self->numBuckets", "hcBucketDefs.h", 67, NULL);
    if (ndxInBucket >= HCBucketDefs_numValues(self, bk))
        HC_assertFail("ndxInBucket < HCBucketDefs_numValues(self, bk)", "hcBucketDefs.h", 68, NULL);
    return self->values[ndxInBucket + self->bucketStart[bk]];
}

HCBucketDefs_s *HCBucketDefs_create(HCMemory_s *mgr, HCInstr_s *lst,
                                    int nBuckets, int nValues)
{
    HCBucketDefs_s *self;
    int tmpMark;
    unsigned short *bucketStart;
    unsigned short *bucketLength;
    unsigned int valCount = (nValues > 0) ? (unsigned int)nValues : 1u;
    int i;

    self = (HCBucketDefs_s *)HCMemory_alloc(
        mgr, (unsigned int)offsetof(HCBucketDefs_s, values) + 2u * valCount);
    self->numBuckets = nBuckets;
    self->bucketStart = (unsigned short *)HCMemory_alloc(
        mgr, 2u * (unsigned int)(nBuckets + 1));

    for (i = 0; i < 16; ++i)
    {
        self->bitVectorCache[i].reg = -1;
        self->bitVectorCache[i].vector = NULL;
    }
    if (nBuckets < 32)
    {
        for (i = 0; i < 16; ++i)
            self->bitVectorCache[i].vector = HCBitSet_create(mgr, nValues);
    }

    tmpMark = HCMemory_mark(mgr);
    bucketStart = self->bucketStart;
    bucketLength = (unsigned short *)HCMemory_alloc(
        mgr, 2u * (unsigned int)(nBuckets + 1));
    memset(bucketStart, 0, 2u * (unsigned int)(nBuckets + 1));
    memset(bucketLength, 0, 2u * (unsigned int)(nBuckets + 1));

    for (i = 0; i < nValues; ++i)
    {
        int v = HC_getDef(&lst[i]);
        if (v != -1)
        {
            if (!(v >= 0 && v < nBuckets))
                HC_assertFail("v >= 0 && v < nBuckets", "hcBucketDefs.c", 78, NULL);
            ++bucketStart[v + 1];
        }
    }
    for (i = 1; i <= nBuckets; ++i)
        bucketStart[i] = (unsigned short)(bucketStart[i] + bucketStart[i - 1]);
    for (i = 0; i < nValues; ++i)
    {
        int v = HC_getDef(&lst[i]);
        if (v != -1)
            self->values[bucketStart[v] + bucketLength[v]++] = (unsigned short)i;
    }

    HCMemory_release(mgr, tmpMark);
    return self;
}

HCBitSet_s *HCBucketDefs_getCachedSet(HCBucketDefs_s *self, int t)
{
    HCBucketDefsCache_s *e = &self->bitVectorCache[t & 0xF];

    if (e->vector == NULL)
        return NULL;
    if (e->reg != t)
    {
        int nElems = HCBucketDefs_numValues(self, t);
        int k;
        HCBitSet_clear(e->vector);
        e->reg = t;
        for (k = 0; k < nElems; ++k)
            HCBitSet_or(e->vector, HCBucketDefs_getValue(self, t, k));
    }
    return e->vector;
}

void HCBucketDefs_sub(HCBucketDefs_s *self, HCBitSet_s *set, int t)
{
    HCBitSet_s *cached;

    if (self == NULL)
        HC_assertFail("self", "hcBucketDefs.c", 131, NULL);
    if (set == NULL)
        HC_assertFail("set", "hcBucketDefs.c", 132, NULL);
    if (t < 0)
        HC_assertFail("t >= 0", "hcBucketDefs.c", 133, NULL);

    cached = HCBucketDefs_getCachedSet(self, t);
    if (cached != NULL)
    {
        HCBitSet_difference(set, cached);
    }
    else
    {
        int s = self->bucketStart[t];
        int nElems = HCBucketDefs_numValues(self, t);
        int k;
        for (k = 0; k < nElems; ++k)
            HCBitSet_sub(set, self->values[k + s]);
    }
}
