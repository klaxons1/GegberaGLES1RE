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

/* --- grComputeTriangleArea (grRasterizer.inl; DLL 25943-25983) --------------
 * Signed screen-space area of the triangle over the vertex cache records'
 * screenPosition x/y (subpixel fixed point), as a 64-bit product-sum. */

HGint64 grComputeTriangleArea(_grVertex *vertexCache, unsigned int indices)
{
    _grVertex *v0 = &vertexCache[grUnpackIndex(indices, 0)];
    _grVertex *v1 = &vertexCache[grUnpackIndex(indices, 1)];
    _grVertex *v2 = &vertexCache[grUnpackIndex(indices, 2)];
    int x0 = v0->screenPosition[0], y0 = v0->screenPosition[1];
    int x1 = v1->screenPosition[0], y1 = v1->screenPosition[1];
    int x2 = v2->screenPosition[0], y2 = v2->screenPosition[1];

    return hgMadd64(hgMul64(x1 - x0, y2 - y0), y1 - y0, x0 - x2);
}

/* --- grRoundTriangleArea (grRasterizer.inl; DLL 00424970) ------------------- */

int grRoundTriangleArea(HGint64 area)
{
    if (hgGet64h(area) < 0)
        return hgLsl64h_0_31(area, 9);
    return -hgLsl64h_0_31(hgNeg64(area), 9);
}

/* --- grQueuePrimitive (grApiFast.inl 742; DLL 32218-32588) ------------------
 * Assembles one primitive (1..3 vertices) into the command queue: cache
 * lookups (the previous primitive's vertices reuse their cache slots), the
 * trivial-reject tests (shared clip plane bits, screen-scissor grouping),
 * face culling (clip-space stp sign, then the screen-area sign) and the
 * 8-byte command {packed cache indices | flags, rounded area}.
 * Command storage: renderStack+168 + 8*commandIndex; the u32 counter at
 * renderStack+1192 counts DWORDS and the flush trigger (grApiFast.inl 1042)
 * fires when it would exceed 254 (128 commands fill the queue region). */

static int queueLookupCacheIndex(Gerbera_s *state, _grVertexCache *cache,
                                 int v, int prev0, int prev1, int prev2)
{
    int ci = -1;

    if (v == prev0)
        ci = cache->prevCacheIndices[0];
    if (v == prev1)
        ci = cache->prevCacheIndices[1];
    if (v == prev2)
        ci = cache->prevCacheIndices[2];
    if (ci == -1)
        ci = grVertexCacheGet(state, v);
    return ci;
}

void grQueuePrimitive(Gerbera_s *state, int i0, int i1, int i2, int numVertices)
{
    _grVertexCache *cache = GR_VERTEX_CACHE(state);
    unsigned int *renderStack = (unsigned int *)state->renderStack;
    _grVertex *vertexBuffer = GR_VERTEX_BUFFER(state);
    int prev0 = cache->prevVertexIndices[0];
    int prev1 = cache->prevVertexIndices[1];
    int prev2 = cache->prevVertexIndices[2];
    int ci0, ci1, ci2, packed, areaSign, triSide, cull;
    unsigned short cm0, cm1, cm2, maskAll;
    _grVertex *v0, *v1, *v2;
    unsigned int *cmd;
    HGint64 area;

    if (numVertices <= 2)
        ci2 = -1;
    else
    {
        cache->prevVertexIndices[2] = i2;
        ci2 = queueLookupCacheIndex(state, cache, i2, prev0, prev1, prev2);
    }
    if (numVertices <= 1)
        ci1 = -1;
    else
    {
        cache->prevVertexIndices[1] = i1;
        ci1 = queueLookupCacheIndex(state, cache, i1, prev0, prev1, prev2);
    }
    cache->prevVertexIndices[0] = i0;
    ci0 = queueLookupCacheIndex(state, cache, i0, prev0, prev1, prev2);
    if (numVertices <= 1)
        ci1 = ci0;
    if (numVertices <= 2)
        ci2 = ci1;
    cache->prevCacheIndices[0] = ci0;
    cache->prevCacheIndices[1] = ci1;
    cache->prevCacheIndices[2] = ci2;
    packed = grSetIndices(ci0, ci1, ci2);
    v0 = &vertexBuffer[ci0];
    v1 = &vertexBuffer[ci1];
    v2 = &vertexBuffer[ci2];
    if (numVertices == 3)
    {
        if ((ci0 != ci1 && ci0 != ci2 && ci1 != ci2) == 0 &&
            grAssert("(ci0 != ci1) && (ci0 != ci2) && (ci1 != ci2)",
                     "grApiFast.inl", 999) != 0)
            grDie(0);
    }
    if (numVertices == 2)
    {
        if (((ci0 != ci1) && (ci1 == ci2)) == 0 &&
            grAssert("(ci0 != ci1) && (ci1 == ci2)", "grApiFast.inl", 1000) != 0)
            grDie(0);
    }
    if (numVertices == 1)
    {
        if (((ci0 == ci1) && (ci1 == ci2)) == 0 &&
            grAssert("(ci0 == ci1) && (ci1 == ci2)", "grApiFast.inl", 1001) != 0)
            grDie(0);
    }
    if (hgSignBit32((254 - GR_COMMAND_QUEUE_INDEX(state)) |
                    (cache->totalFreeSlots - numVertices)) != 0)
        grFlushPrimitiveBatch(state);
    cm0 = v0->clipMask;
    cm1 = v1->clipMask;
    cm2 = v2->clipMask;
    if ((cm0 & cm1 & cm2 & 0xFFC3u) != 0)
        return;
    maskAll = cm0 | cm1 | cm2;
    if (hgInRange32(numVertices, 1, 3) == 0 &&
        grAssert("hgInRange32(numVertices, 1, 3)", "grApiFast.inl", 1040) != 0)
        grDie(0);
    if (numVertices == 3 && grTestCapability(state, GR_CAPABILITY_CULL_FACE) != 0)
    {
        int stp = IVector4_stp(&v0->clipPosition, &v1->clipPosition, &v2->clipPosition);

        if (stp != 0 && (unsigned int)stp >> 31 == state->cullNegative)
            return;
    }
    cmd = renderStack + GR_COMMAND_QUEUE_INDEX(state) + 42;
    if ((maskAll & 0xFC3Fu) != 0)
    {
        cmd[0] = (unsigned int)packed | 0x10000000u;
        cmd[1] = 0;
        GR_COMMAND_QUEUE_INDEX(state) += 2;
    }
    else
    {
        _grRect *sc = &state->renderScissor;
        int sx0, sx1, sx2, sy0, sy1, sy2, m, g;

        if ((v0->stateMask & GR_VERTEXMASK_VIEWPORT_TRANSFORMED) == 0)
            grViewportTransformVertex(state, v0);
        if (numVertices > 1 && (v1->stateMask & GR_VERTEXMASK_VIEWPORT_TRANSFORMED) == 0)
            grViewportTransformVertex(state, v1);
        if (numVertices > 2 && (v2->stateMask & GR_VERTEXMASK_VIEWPORT_TRANSFORMED) == 0)
            grViewportTransformVertex(state, v2);
        sx0 = grSubpixelTrunc(v0->screenPosition[0]);
        sx1 = numVertices > 1 ? grSubpixelTrunc(v1->screenPosition[0]) : 0;
        sx2 = numVertices > 2 ? grSubpixelTrunc(v2->screenPosition[0]) : 0;
        sy0 = grSubpixelTrunc(v0->screenPosition[1]);
        sy1 = numVertices > 1 ? grSubpixelTrunc(v1->screenPosition[1]) : 0;
        sy2 = numVertices > 2 ? grSubpixelTrunc(v2->screenPosition[1]) : 0;
        m = sx0 - sc->x0;
        if (numVertices > 1)
            m &= sx1 - sc->x0;
        if (numVertices > 2)
            m &= sx2 - sc->x0;
        g = sc->x1 - sx0;
        if (numVertices > 1)
            g &= sc->x1 - sx1;
        if (numVertices > 2)
            g &= sc->x1 - sx2;
        m |= g;
        g = sy0 - sc->y0;
        if (numVertices > 1)
            g &= sy1 - sc->y0;
        if (numVertices > 2)
            g &= sy2 - sc->y0;
        m |= g;
        g = sc->y1 - sy0;
        if (numVertices > 1)
            g &= sc->y1 - sy1;
        if (numVertices > 2)
            g &= sc->y1 - sy2;
        m |= g;
        if (hgSignBit32(m) != 0)
            return;
        if (numVertices == 3)
        {
            area = grComputeTriangleArea(vertexBuffer, (unsigned int)packed);
            areaSign = (unsigned int)hgGet64h(area) >> 31;
            triSide = areaSign ^ grGetFrontFace(state);
            cull = (areaSign ^ state->cullNegative) &
                   grTestCapability(state, GR_CAPABILITY_CULL_FACE);
            if (cull == 0 && hgIsZero64(area) == 0)
            {
                cmd[0] = (unsigned int)packed | ((unsigned int)triSide << 24);
                cmd[1] = (unsigned int)grRoundTriangleArea(area);
                GR_COMMAND_QUEUE_INDEX(state) += 2;
            }
        }
        else
        {
            cmd[0] = (unsigned int)packed;
            cmd[1] = 0;
            GR_COMMAND_QUEUE_INDEX(state) += 2;
        }
    }
}

/* --- grQueuePrimitive1/2/3 (DLL 32591-32618) --------------------------------
 * Degenerate-primitive rejection wrappers used by grFetchPrimitives. */

void grQueuePrimitive3(Gerbera_s *state, int i0, int i1, int i2)
{
    if (i0 != i2 && i1 != i2 && i0 != i1)
        grQueuePrimitive(state, i0, i1, i2, 3);
}

void grQueuePrimitive2(Gerbera_s *state, int i0, int i1)
{
    if (i0 != i1)
        grQueuePrimitive(state, i0, i1, i1, 2);
}

void grQueuePrimitive1(Gerbera_s *state, int i0)
{
    grQueuePrimitive(state, i0, i0, i0, 1);
}

/* --- grFlushPrimitiveBatch (grApiFast.inl 490; DLL 31970-32124) -------------
 * Drains the command queue: builds the two fetch lists (the smooth/provoking
 * list at renderStack+1264 for colors+normals+lighting, the per-vertex list
 * at +1196 for texcoords+point size+fog; list bits 0x20/0x10 in stateMask),
 * dispatches each command to the rasterizer (clip path via grClipPrimitive)
 * and re-arms the cache for the next batch (the previous primitive's slots
 * stay locked). */

void grFlushPrimitiveBatch(Gerbera_s *state)
{
    unsigned char *renderStack = (unsigned char *)state->renderStack;
    unsigned int *queue = (unsigned int *)(renderStack + 168);
    unsigned int *queueEnd = queue + GR_COMMAND_QUEUE_INDEX(state);
    unsigned char *smoothList = renderStack + 1264;
    unsigned char *pipeList = renderStack + 1196;
    _grVertex *vertexBuffer = GR_VERTEX_BUFFER(state);
    _grVertexCache *cache = GR_VERTEX_CACHE(state);
    int smooth = grTestCapability(state, GR_CAPABILITY_SMOOTH_SHADING);
    int primitiveVertices = grGetPrimitiveVertices(state->primitiveType);
    unsigned int *cmd = queueEnd - 2;
    unsigned int *dispatch;
    HGint64 mask;
    int j;

    grUpdateVertexPipeline(state);
    while (cmd >= queue)
    {
        unsigned int packed = *cmd;
        int first = 1;

        cmd -= 2;
        for (j = 0; j < primitiveVertices; ++j)
        {
            int idx = grUnpackIndex(packed, j);
            _grVertex *vertex = &vertexBuffer[idx];
            unsigned short sm = vertex->stateMask;

            if (first != 0 && (sm & 0x20) == 0)
            {
                sm |= 0x20;
                vertex->stateMask = sm;
                *smoothList++ = (unsigned char)idx;
            }
            if ((sm & 0x10) == 0)
            {
                sm |= 0x10;
                vertex->stateMask = sm;
                *pipeList++ = (unsigned char)idx;
            }
            first = smooth;
        }
    }
    *smoothList = 0xFF;
    *pipeList = 0xFF;
    if (renderStack[1264] != 0xFF)
    {
        if (state->fetchVertexColor != NULL)
            state->fetchVertexColor(state, renderStack + 1264);
        if (state->fetchVertexNormal != NULL)
        {
            state->fetchVertexNormal(state, renderStack + 1264);
            grApplyLighting(state, renderStack + 1264);
        }
    }
    if (renderStack[1196] != 0xFF)
    {
        if (state->fetchTexCoord[0] != NULL)
            state->fetchTexCoord[0](state, renderStack + 1196, 0);
        if (state->fetchTexCoord[1] != NULL)
            state->fetchTexCoord[1](state, renderStack + 1196, 1);
        if (state->fetchPointSize != NULL)
            state->fetchPointSize(state, renderStack + 1196);
        if (state->applyFog != NULL)
            state->applyFog(state, renderStack + 1196);
    }
    for (dispatch = queue; dispatch < queueEnd; )
    {
        _grClipTriangle tri;
        unsigned int cmd0 = *dispatch++;
        unsigned int cmd1 = *dispatch++;

        if (dispatch > queueEnd &&
            grAssert("cmdQueue <= cmdQueueEnd", "grApiFast.inl", 676) != 0)
            grDie(0);
        tri.indices = cmd0;
        tri.area = (int)cmd1;
        tri.side = (int)((cmd0 >> 24) & 1);
        if ((cmd0 & 0x10000000u) != 0)
            grClipPrimitive(state, cmd0, grGetPrimitiveVertices(state->primitiveType));
        else if (state->primitiveType == GR_PRIMITIVETYPE_POINT)
            grrDrawClippedPoint(state, &tri, cmd0);
        else if (state->primitiveType == GR_PRIMITIVETYPE_LINE)
            grrDrawClippedLine(state, &tri, cmd0);
        else
        {
            if (state->primitiveType != GR_PRIMITIVETYPE_TRIANGLE &&
                grAssert("state->primitiveType == GR_PRIMITIVETYPE_TRIANGLE",
                         "grApiFast.inl", 687) != 0)
                grDie(0);
            grrDrawClippedTriangle(state, &tri, cmd0);
        }
    }
    mask = hgSetBit64(hgSetBit64(hgSetBit64(hgZero64(), cache->prevCacheIndices[0]),
                                 cache->prevCacheIndices[1]),
                      cache->prevCacheIndices[2]);
    cache->batchLockMask = mask;
    cache->totalFreeSlots = GR_VERTEX_CACHE_SIZE - primitiveVertices;
    cache->allocIndex = GR_VERTEX_CACHE_SIZE;
    GR_COMMAND_QUEUE_INDEX(state) = 0;
}

/* --- grCheckPixelPipeStack (grApiFast.inl 486; DLL 31896-31967) --------------
 * Validates the live pixel-pipe record against the draw surface strides and
 * the packed rasterizer state (asserts 491-500). */

void grCheckPixelPipeStack(Gerbera_s *state)
{
    _grPixelPipe *pixelPipe = (_grPixelPipe *)state->pixelPipeline;
    int ok = 1;

    if (pixelPipe->colorBufferStride != ((int *)&state->drawSurface)[7] &&
        grAssert("pixelPipe->colorBufferStride == drawSurface->buffers[GR_BUFFERTYPE_COLOR].stride",
                 "grApiFast.inl", 491) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->depthBufferStride != ((int *)&state->drawSurface)[9] &&
        grAssert("pixelPipe->depthBufferStride == drawSurface->buffers[GR_BUFFERTYPE_DEPTH].stride",
                 "grApiFast.inl", 492) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->stencilBufferStride != ((int *)&state->drawSurface)[11] &&
        grAssert("pixelPipe->stencilBufferStride == drawSurface->buffers[GR_BUFFERTYPE_STENCIL].stride",
                 "grApiFast.inl", 493) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->fogColor != state->packedFogColor &&
        grAssert("pixelPipe->fogColor == state->packedFogColor", "grApiFast.inl", 495) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->alphaReference != state->alphaReference &&
        grAssert("pixelPipe->alphaReference == state->alphaReference", "grApiFast.inl", 496) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->stencilReference != state->stencilReference &&
        grAssert("pixelPipe->stencilReference == state->stencilReference", "grApiFast.inl", 497) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->stencilReferenceMasked !=
            (unsigned char)(state->stencilReference & state->stencilTestMask) &&
        grAssert("pixelPipe->stencilReferenceMasked == (state->stencilReference & state->stencilTestMask)",
                 "grApiFast.inl", 498) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->stencilTestMask != state->stencilTestMask &&
        grAssert("pixelPipe->stencilTestMask == state->stencilTestMask", "grApiFast.inl", 499) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (pixelPipe->stencilWriteMask != state->stencilWriteMask &&
        grAssert("pixelPipe->stencilWriteMask == state->stencilWriteMask", "grApiFast.inl", 500) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
}

/* --- grFetchPrimitives (grApiFast.inl 1114; DLL 32611-32933) -----------------
 * Walks the index source per fetch type and queues one primitive per step
 * through the grQueuePrimitive1/2/3 wrappers (degenerates drop out there);
 * marks the vertex pipeline dirty, then flushes the batch.
 * TRIANGLES/STRIP/FAN keep the decompile's rotated queue order
 * qp3(src[2], src[0], src[1]); strip winding flips per segment. */

void grFetchPrimitives(Gerbera_s *state, int fetchType, int numVertices,
                       int vertexCount, void *srcIndices, int baseIndex)
{
    const unsigned char *bytes = (const unsigned char *)srcIndices;
    const unsigned short *shorts = (const unsigned short *)srcIndices;
    int vtxIndex = baseIndex;
    int stripSwap = 0;
    int fetchIndex = 0;
    int ok = 1;

    state->vertexPipelineDirty = 1;
    if (state == NULL && grAssert("state", "grApiFast.inl", 1298) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (hgInRange32(fetchType, 0, GR_FETCHTYPE_MAX - 1) == 0 &&
        grAssert("hgInRange32(fetchType, 0, GR_FETCHTYPE_MAX-1)", "grApiFast.inl", 1299) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (vertexCount < numVertices &&
        grAssert("vertexCount >= numVertices", "grApiFast.inl", 1300) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    switch (fetchType)
    {
        case GR_FETCHTYPE_POINTSchar:
            do
            {
                grQueuePrimitive1(state, *bytes++);
                --vertexCount;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_POINTS_SHORT:
            do
            {
                grQueuePrimitive1(state, *shorts++);
                --vertexCount;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_POINTS_ARRAY:
            do
            {
                grQueuePrimitive1(state, vtxIndex++);
                --vertexCount;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_LINESchar:
            do
            {
                grQueuePrimitive2(state, bytes[1], bytes[0]);
                bytes += 2;
                vertexCount -= 2;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_LINES_SHORT:
            do
            {
                grQueuePrimitive2(state, shorts[1], shorts[0]);
                shorts += 2;
                vertexCount -= 2;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_LINES_ARRAY:
            do
            {
                int i0 = vtxIndex;
                int i1 = vtxIndex + 1;

                vtxIndex += 2;
                vertexCount -= 2;
                grQueuePrimitive2(state, i1, i0);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_LINELOOPchar:
            do
            {
                grQueuePrimitive2(state, bytes[1], bytes[0]);
                bytes += 1;
                --vertexCount;
            } while (vertexCount >= numVertices);
            grQueuePrimitive2(state, ((const unsigned char *)srcIndices)[0], bytes[0]);
            break;
        case GR_FETCHTYPE_LINELOOP_SHORT:
            do
            {
                grQueuePrimitive2(state, shorts[1], shorts[0]);
                ++shorts;
                --vertexCount;
            } while (vertexCount >= numVertices);
            grQueuePrimitive2(state, ((const unsigned short *)srcIndices)[0], shorts[0]);
            break;
        case GR_FETCHTYPE_LINELOOP_ARRAY:
            do
            {
                int i1 = vtxIndex++;

                --vertexCount;
                grQueuePrimitive2(state, vtxIndex, i1);
            } while (vertexCount >= numVertices);
            grQueuePrimitive2(state, baseIndex, vtxIndex);
            break;
        case GR_FETCHTYPE_LINESTRIPchar:
            do
            {
                grQueuePrimitive2(state, bytes[1], bytes[0]);
                bytes += 1;
                --vertexCount;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_LINESTRIP_SHORT:
            do
            {
                grQueuePrimitive2(state, shorts[1], shorts[0]);
                ++shorts;
                --vertexCount;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_LINESTRIP_ARRAY:
            do
            {
                int i1 = vtxIndex++;

                --vertexCount;
                grQueuePrimitive2(state, vtxIndex, i1);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRIANGLESchar:
            do
            {
                grQueuePrimitive3(state, bytes[2], bytes[0], bytes[1]);
                bytes += 3;
                vertexCount -= 3;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRIANGLES_SHORT:
            do
            {
                grQueuePrimitive3(state, shorts[2], shorts[0], shorts[1]);
                shorts += 3;
                vertexCount -= 3;
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRIANGLES_ARRAY:
            do
            {
                int i0 = vtxIndex;
                int i1 = vtxIndex + 1;
                int i2 = vtxIndex + 2;

                vtxIndex += 3;
                vertexCount -= 3;
                grQueuePrimitive3(state, i2, i0, i1);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRISTRIPchar:
            do
            {
                int i1 = bytes[stripSwap];
                int i2 = bytes[1 - stripSwap];
                int i0 = bytes[2];

                bytes += 1;
                stripSwap = 1 - stripSwap;
                --vertexCount;
                grQueuePrimitive3(state, i0, i1, i2);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRISTRIP_SHORT:
            do
            {
                int i1 = shorts[stripSwap];
                int i2 = shorts[1 - stripSwap];
                int i0 = shorts[2];

                ++shorts;
                stripSwap = 1 - stripSwap;
                --vertexCount;
                grQueuePrimitive3(state, i0, i1, i2);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRISTRIP_ARRAY:
            do
            {
                int i1 = stripSwap + vtxIndex;
                int i2 = vtxIndex + 1 - stripSwap;
                int i0 = vtxIndex + 2;

                stripSwap = 1 - stripSwap;
                ++vtxIndex;
                --vertexCount;
                grQueuePrimitive3(state, i0, i1, i2);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRIFANchar:
            do
            {
                int i2 = bytes[fetchIndex + 1];
                int i0 = bytes[fetchIndex + 2];

                ++fetchIndex;
                --vertexCount;
                grQueuePrimitive3(state, i0, bytes[0], i2);
            } while (vertexCount >= numVertices);
            break;
        case GR_FETCHTYPE_TRIFAN_SHORT:
            do
            {
                int i2 = shorts[fetchIndex + 1];
                int i0 = shorts[fetchIndex + 2];

                ++fetchIndex;
                --vertexCount;
                grQueuePrimitive3(state, i0, shorts[0], i2);
            } while (vertexCount >= numVertices);
            break;
        default:
            if (fetchType != GR_FETCHTYPE_TRIFAN_ARRAY &&
                grAssert("fetchType == GR_FETCHTYPE_TRIFAN_ARRAY", "grApiFast.inl", 1562) != 0)
                ok = 0;
            if (ok == 0)
                grDie(0);
            do
            {
                int i2 = fetchIndex + baseIndex + 1;
                int i0 = fetchIndex + baseIndex + 2;

                ++fetchIndex;
                --vertexCount;
                grQueuePrimitive3(state, i0, baseIndex, i2);
            } while (vertexCount >= numVertices);
            break;
    }
    grFlushPrimitiveBatch(state);
}

/* --- grRenderElements (grApiFast.inl 1596; DLL 32936-33023) ----------------- */

void grRenderElements(Gerbera_s *state, int primitiveType, int vertexCount,
                      int dataType, void *indices)
{
    int numVertices;
    int fetchType;
    int ok = 1;

    if (state->compilationFailed != 0)
        return;
    if (state == NULL && grAssert("state", "grApiFast.inl", 1606) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (hgInRange32(primitiveType, 0, GR_PRIMITIVE_MAX - 1) == 0 &&
        grAssert("hgInRange32(primitiveType, 0, GR_PRIMITIVE_MAX-1)", "grApiFast.inl", 1607) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (vertexCount <= 0 && grAssert("vertexCount > 0", "grApiFast.inl", 1608) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (dataType != GR_DATATYPE_UINT8 && dataType != GR_DATATYPE_UINT16 &&
        grAssert("dataType==GR_DATATYPE_UINT8 || dataType==GR_DATATYPE_UINT16",
                 "grApiFast.inl", 1609) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (indices == NULL && grAssert("indices", "grApiFast.inl", 1610) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    numVertices = 3;
    if (primitiveType == GR_PRIMITIVE_POINTS)
    {
        numVertices = 1;
    }
    else if (hgInRange32(primitiveType, 1, 3) != 0)
    {
        numVertices = 2;
    }
    else if (hgInRange32(primitiveType, 4, 6) == 0)
    {
        if (grAssert("hgInRange32(primitiveType, GR_PRIMITIVE_TRIANGLES, GR_PRIMITIVE_TRIANGLE_FAN)",
                     "grApiFast.inl", 1615) != 0)
            grDie(0);
    }
    fetchType = 3 * primitiveType;
    if (dataType == GR_DATATYPE_UINT16)
    {
        ++fetchType;
    }
    else if (dataType != GR_DATATYPE_UINT8)
    {
        if (grAssert("dataType == GR_DATATYPE_UINT8", "grApiFast.inl", 1619) != 0)
            grDie(0);
    }
    grFetchPrimitives(state, fetchType, numVertices, vertexCount, indices, 0);
    grCheckPixelPipeStack(state);
}

/* --- grRenderArrays (grApiFast.inl 1636; DLL 33026-33074) ------------------- */

void grRenderArrays(Gerbera_s *state, int primitiveType, int vertexCount, int baseIndex)
{
    int fetchType = 3 * primitiveType + 2;
    int ok = 1;

    if (state->compilationFailed != 0)
        return;
    if (hgInRange32(primitiveType, 0, GR_PRIMITIVE_MAX - 1) == 0 &&
        grAssert("hgInRange32(primitiveType, 0, GR_PRIMITIVE_MAX-1)", "grApiFast.inl", 1647) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (vertexCount <= 0 && grAssert("vertexCount > 0", "grApiFast.inl", 1648) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (primitiveType == GR_PRIMITIVE_POINTS)
    {
        grFetchPrimitives(state, fetchType, 1, vertexCount, NULL, baseIndex);
    }
    else if (hgInRange32(primitiveType, 1, 3) != 0)
    {
        grFetchPrimitives(state, fetchType, 2, vertexCount, NULL, baseIndex);
    }
    else if (primitiveType == GR_PRIMITIVE_TEXTURE_RECT)
    {
        grFetchPrimitives(state, fetchType, 0, vertexCount, NULL, baseIndex);
    }
    else
    {
        if (hgInRange32(primitiveType, 4, 6) == 0 &&
            grAssert("hgInRange32(primitiveType, GR_PRIMITIVE_TRIANGLES, GR_PRIMITIVE_TRIANGLE_FAN)",
                     "grApiFast.inl", 1656) != 0)
            grDie(0);
        grFetchPrimitives(state, fetchType, 3, vertexCount, NULL, baseIndex);
    }
}
