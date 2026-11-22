/* grApiFast.c - the grFast command-queue vertex cache (port of the
 * decompiled grApiFast.inl vertex-cache functions).
 *
 * The cache lives at renderStack+0 (grSetupRendering assigns
 * state->vertexCache = state->renderStack) and keeps a 32-entry hash table
 * of packed cache-index bytes (4 slots per bucket), a 64-bit per-slot lock
 * mask and a free-slot counter. grVertexCacheGet maps a raw vertex index to
 * a vertex-buffer slot (hash bucket -> 4-way byte scan -> lock/evict), and
 * runs the full transform pipeline (grFetchWeightAndIndex +
 * grTransformVertex) for cache misses.
 *
 * Assert sites keep the original file/line strings.
 */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "imath/imApi.h"
#include "hg/hgInt32.h"
#include "hg/hgFill.h"

#define GR_VERTEX_CACHE(state) ((_grVertexCache *)(state)->vertexCache)
#define GR_VERTEX_BUFFER(state) \
    ((_grVertex *)((unsigned char *)(state)->renderStack + 1332))
#define GR_COMMAND_QUEUE_INDEX(state) \
    (*(int *)((unsigned char *)(state)->renderStack + GR_RENDER_STACK_COMMAND_QUEUE_INDEX))

/* --- grVertexCacheInvalidate (grApiFast.inl 48; DLL 31524-31559) ------------ */

void grVertexCacheInvalidate(Gerbera_s *state)
{
    _grVertexCache *cache;
    int ok = 1;
    int cond = state->renderStackAllocated != 0 && state->renderStack != NULL;

    if (!cond && grAssert("state->renderStackAllocated && state->renderStack",
                          "grApiFast.inl", 59) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (state->vertexCache == NULL &&
        grAssert("state->vertexCache", "grApiFast.inl", 60) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    cache = GR_VERTEX_CACHE(state);
    GR_COMMAND_QUEUE_INDEX(state) = 0;
    cache->batchLockMask = hgZero64();
    cache->prevVertexIndices[0] = -1;
    cache->prevVertexIndices[1] = -1;
    cache->prevVertexIndices[2] = -1;
    cache->totalFreeSlots = GR_VERTEX_CACHE_SIZE;
    cache->allocIndex = GR_VERTEX_CACHE_SIZE;
    cache->prevCacheIndices[0] = 0;
    cache->prevCacheIndices[1] = 0;
    cache->prevCacheIndices[2] = 0;
    GR_VERTEX_BUFFER(state)[0].index = -1;
    hgFill8((unsigned char *)cache->cacheIndices, 0, 128);
}

/* --- grVertexCacheGet (grApiFast.inl 648; DLL 32127-32215) ------------------
 * slot hash = (v ^ v>>3 ^ v>>5); the bucket is the low 5 bits, the eviction
 * slot the low 6 bits. A hit brings the matching byte to byte 0 (hgRor32);
 * a miss takes the low-6 slot, or - when locked - the highest unlocked slot
 * (63 - clz64(~mask), assert 793), fills the record and transforms it.
 * assert 830 guards the returned cache index range. */

int grVertexCacheGet(Gerbera_s *state, int vtxIndex)
{
    _grVertexCache *cache = GR_VERTEX_CACHE(state);
    _grVertex *vertexBuffer = GR_VERTEX_BUFFER(state);
    unsigned int hash = (unsigned int)(vtxIndex ^ (vtxIndex >> 3) ^ (vtxIndex >> 5));
    unsigned int entry = cache->cacheIndices[hash & 0x1F];
    int slot;

    if (vtxIndex != vertexBuffer[grUnpackIndex(entry, 0)].index)
    {
        if (vtxIndex == vertexBuffer[grUnpackIndex(entry, 1)].index)
        {
            entry = (unsigned int)hgRor32((int)entry, 8);
        }
        else if (vtxIndex == vertexBuffer[grUnpackIndex(entry, 2)].index)
        {
            entry = (unsigned int)hgRor32((int)entry, 16);
        }
        else if (vtxIndex == vertexBuffer[grUnpackIndex(entry, 3)].index)
        {
            entry = (unsigned int)hgRor32((int)entry, 24);
        }
        else
        {
            slot = (int)(hash & 0x3F);
            if (hgGetBit64(cache->batchLockMask, slot) != 0)
            {
                HGint64 notMask = hgNot64(cache->batchLockMask);

                slot = 63 - hgClz64(notMask);
                if (hgInRange32(slot, 0, GR_VERTEX_CACHE_SIZE - 1) == 0 &&
                    grAssert("hgInRange32(cIndex, 0, GR_VERTEX_CACHE_SIZE-1)",
                             "grApiFast.inl", 793) != 0)
                    grDie(0);
            }
            entry = (entry << 8) | (unsigned int)slot;
            vertexBuffer[slot].index = vtxIndex;
            vertexBuffer[slot].stateMask = 0;
            if (grTestCapability(state, GR_CAPABILITY_MATRIX_PALETTE) != 0 &&
                grTestCapability(state, GR_CAPABILITY_MATRIX_INDEX_ARRAY) != 0 &&
                grTestCapability(state, GR_CAPABILITY_WEIGHT_ARRAY) != 0 &&
                state->vertexArray[3].data != NULL &&
                state->vertexArray[4].data != NULL)
            {
                vertexBuffer[slot].weightIndices =
                    grFetchWeightAndIndex(state, vertexBuffer[slot].weights, vtxIndex);
            }
            grTransformVertex(state, &vertexBuffer[slot]);
        }
    }
    cache->cacheIndices[hash & 0x1F] = entry;
    slot = (int)(entry & 0xFF);
    if (hgInRange32(slot, 0, GR_VERTEX_CACHE_SIZE - 1) == 0 &&
        grAssert("hgInRange32(cIndex, 0, GR_VERTEX_CACHE_SIZE-1)",
                 "grApiFast.inl", 830) != 0)
        grDie(0);
    {
        HGint64 mask = cache->batchLockMask;

        cache->totalFreeSlots -= (int)(hgGetBit64(mask, slot) ^ 1);
        cache->batchLockMask = hgSetBit64(mask, slot);
    }
    return slot;
}
