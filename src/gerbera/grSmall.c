/* grSmall.c - small core utilities (start of the port; grows as pieces of
 * the original grSmall.c are brought over). */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "hg/hgInt32.h"
#include <stdint.h>
#ifdef __SYMBIAN32__
#include "symbian_os.h"
#endif

/* grIsAligned - verbatim (docs/libGLES_CM.dll.c:13072-13075, 3 TU copies) */
int grIsAligned(void *ptr, int alignment)
{
    return ((uintptr_t)(alignment - 1) & (uintptr_t)ptr) == 0;
}

/* grIsValidBool - verbatim (docs/libGLES_CM.dll.c:9187-9195, 2 TU copies) */
int grIsValidBool(int v)
{
    if (v == 1)
        return 1;
    return v == 0;
}

/* --- grComputeGradientReference (grSmall.c 153; DLL 25334-25342) ----------- */

int grComputeGradientReference(int v0sx, int v0sy, int xAdd, int yAdd)
{
    HGint64 v4 = hgMul64(v0sx, xAdd);
    HGint64 v5 = hgMadd64(v4, v0sy, yAdd);

    return -hgLsl64h_0_31(v5, 20);
}

/* --- grComputeDepthOffset (grSmall.c 165; DLL 25902-25940) ------------------
 * Adjusts the depth-gradient reference for polygon offset: units scale with
 * the depth-buffer format (byte at drawSurface+10 = depthBytes), factor
 * scales with the steeper of the two depth adders. */
void grComputeDepthOffset(Gerbera_s *state)
{
    _grPixelPipe *pixelPipeline = (_grPixelPipe *)state->pixelPipeline;
    int units = state->depthOffsetUnits;
    int ref = units + pixelPipeline->reference[0];
    int factor = state->depthOffsetFactor;

    switch (((unsigned char *)&state->drawSurface)[10])   /* depthBytes */
    {
        case 1:
            ref += units << 8;
            break;
        case 2:
            ref += units;
            break;
        case 4:
            ref += units >> 16;
            break;
        default:
            break;
    }
    if (factor != 0)
    {
        int xAdder = hgAbs32(pixelPipeline->xAdder[0]);
        int yAdder = hgAbs32(pixelPipeline->yAdder[0]);
        int v1 = hgMax32(xAdder, yAdder);
        HGint64 v2 = hgMul64(factor, v1);

        ref += hgLsl64h(v2, 16);
    }
    pixelPipeline->reference[0] = ref;
}

/* --- g_mutableParamsMask (data.fragments.c 33) -------------------------------
 * Bitmap over the 78 mutable pixel-pipe parameters: the draw poison loop
 * clobbers every mutable slot between draws so stale state cannot leak. */

const unsigned char g_mutableParamsMask[12] =
{
    0xFE, 0xFF, 0xFE, 0xFF, 0x00, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00
};

/* --- grDrawClippedTriangle (grSmall.c 254; DLL 26251-26388) ------------------
 * y-sorts the triangle's three vertex records, scan-converts them, then per
 * draw: sets the color/depth/stencil row pointers for the first scanline,
 * pre-advances the active gradients to yStart and runs the compiled pixel
 * pipeline over the scanlines. emptyBits = the number of LOW ZERO bits in
 * the 20.12 screen coordinates (8 from grrDrawClippedTriangle's affine
 * path: the viewport transform leaves 8 pad bits). */

#define GR_STACK_CHECK_MAGIC0 (-559038737)
#define GR_STACK_CHECK_MAGIC1 (-1073628482)
#define GR_MUTABLE_PARAM_COUNT 78

void grDrawClippedTriangle(Gerbera_s *state, _grClipTriangle *tri,
                           unsigned int origIndices, int emptyBits)
{
    _grVertex *vertexBuffer = (_grVertex *)((unsigned char *)state->renderStack + 1332);
    _grVertex *sorted[3];
    _grVertex *swap;
    unsigned int magic0 = GR_STACK_CHECK_MAGIC0;
    unsigned int magic1 = GR_STACK_CHECK_MAGIC1;
    unsigned int activeGradientMask;
    int flatShadeIndex = (int)(grUnpackIndex(origIndices, 0));
    int i;

    (void)magic0;
    (void)magic1;
    sorted[0] = vertexBuffer + grUnpackIndex(tri->indices, 0);
    sorted[1] = vertexBuffer + grUnpackIndex(tri->indices, 1);
    sorted[2] = vertexBuffer + grUnpackIndex(tri->indices, 2);
    /* y-sort (record +32 = screenPosition[1]); stable 3-sort keeping the
     * original order for equal y */
    swap = sorted[0];
    if (sorted[1]->screenPosition[1] < sorted[0]->screenPosition[1])
        sorted[0] = sorted[1];
    if (sorted[2]->screenPosition[1] < sorted[0]->screenPosition[1])
    {
        sorted[0] = sorted[2];
        sorted[2] = swap;
    }
    if (sorted[0] == sorted[1])
        sorted[1] = swap;
    if (sorted[2]->screenPosition[1] < sorted[1]->screenPosition[1])
    {
        swap = sorted[1];
        sorted[1] = sorted[2];
        sorted[2] = swap;
    }

    if (grScanConvertTriangle(state, (_grRasterPoint *)&sorted[0]->screenPosition[0],
                              (_grRasterPoint *)&sorted[1]->screenPosition[0],
                              (_grRasterPoint *)&sorted[2]->screenPosition[0],
                              emptyBits) != 0)
    {
        _grPixelPipe *pixelPipe = state->pixelPipeline;
        int yStart = pixelPipe->yStart;

        activeGradientMask = (unsigned int)state->activeGradientMask;
        grSetupTriangleGradients(state, tri, flatShadeIndex, activeGradientMask);
        for (i = 0; i < 3; ++i)
        {
            unsigned short stateMask =
                vertexBuffer[grUnpackIndex(tri->indices, i)].stateMask;

            if ((stateMask & 0x2C) == 0 &&
                grAssert("stateMask & (GR_VERTEXMASK_TRANSFORMED | GR_VERTEXMASK_VIEWPORT_TRANSFORMED | GR_VERTEXMASK_COLOR_COMPUTED)",
                         "grRasterizer.inl", 853) != 0)
                grDie(0);
            if ((stateMask & 0xC0) != 0 &&
                grAssert("!(stateMask & (GR_VERTEXMASK_INTERPOLATE_GRADIENTS | GR_VERTEXMASK_FORCE_CULL))",
                         "grRasterizer.inl", 854) != 0)
                grDie(0);
        }
        pixelPipe->colorRow = (void *)((char *)state->drawSurface.buffers[0].data +
                                       state->drawSurface.buffers[0].stride * yStart);
        pixelPipe->depthRow = (void *)((char *)state->drawSurface.buffers[1].data +
                                       state->drawSurface.buffers[1].stride * yStart);
        pixelPipe->stencilRow = (void *)((char *)state->drawSurface.buffers[2].data +
                                         state->drawSurface.buffers[2].stride * yStart);
        for (i = state->numActiveGradients - 1; i >= 0; --i)
        {
            int g = state->activeGradientIndex[i];

            pixelPipe->reference[g] = (int)((unsigned int)pixelPipe->reference[g] +
                                          (unsigned int)pixelPipe->yAdder[g] * (unsigned int)yStart);
        }
        grExecutePixelPipeline(state);
#if HG_ASSERT_ENABLED
        for (i = 0; i < GR_MUTABLE_PARAM_COUNT; ++i)
        {
            if ((g_mutableParamsMask[i >> 3] & (1 << (i & 7))) != 0)
                pixelPipe->mutableParams[i] = -623195394;
        }
#endif
        /* the decompile checks __grMemoryCheck[] canaries on the C stack
         * here (grRasterizer.inl 906); hosted builds have no smashed-stack
         * window to re-check - magic0/magic1 keep the locals alive */
    }
}

/* --- grICacheFlush (grSmall.c; DLL 42318-42321) -------------------------------
 * EPOC: User::IMB_Range(start, start + numBytes) - instruction-cache
 * maintenance over freshly written JIT code. Hosted builds run coherent
 * caches, so this is a no-op. */

void grICacheFlush(void *start, unsigned int numBytes)
{
#ifdef __SYMBIAN32__
    if (start != NULL && numBytes != 0u)
        sym_User_IMB_Range(start, (unsigned char *)start + numBytes);
#elif defined(__arm__) && defined(__GNUC__)
    if (start != NULL && numBytes != 0u)
        __builtin___clear_cache((char *)start, (char *)start + numBytes);
#else
    (void)start;
    (void)numBytes;
#endif
}

/* --- grSmall.c prefix getters (DLL 34729-34780) -------------------------------
 * Plain state-field readers used by grEvaluatePixelPipeline and the API
 * query paths. The decompile declares them returning the enum types; the
 * port returns int. */

int grGetAlphaFunc(Gerbera_s *state)
{
    return state->alphaFunc;
}

int grGetSrcBlendFactor(Gerbera_s *state)
{
    return state->srcBlendFactor;
}

int grGetDstBlendFactor(Gerbera_s *state)
{
    return state->dstBlendFactor;
}

int grGetStencilFunc(Gerbera_s *state)
{
    return state->stencilFunc;
}

int grGetStencilFail(Gerbera_s *state)
{
    return state->stencilFail;
}

int grGetStencilZFail(Gerbera_s *state)
{
    return state->stencilZFail;
}

int grGetStencilZPass(Gerbera_s *state)
{
    return state->stencilZPass;
}

int grGetLogicOp(Gerbera_s *state)
{
    return state->logicOp;
}

int grGetRectGradients(Gerbera_s *state)
{
    return state->screenRectGradients;
}

/* --- grGetMinWorkMemorySize / grGetMinSharedMemorySize (grSmall.c) ---------
 * On 32-bit x86/ARM, sizeof(Gerbera_s) == 7664 and 32 * sizeof(_grCacheEntry)
 * == 2048, totaling 9712. On 64-bit hosted builds pointers widen the structs,
 * so the aligned struct sizes are used (which evaluate to 9712 on 32-bit). */

int grGetMinWorkMemorySize(void)
{
    int stateBytes = ((int)sizeof(Gerbera_s) + 7) & ~7;
    int entryBytes = 32 * (((int)sizeof(_grCacheEntry) + 7) & ~7);

    return hgMax32(9712, stateBytes + entryBytes);
}

int grGetMinSharedMemorySize(void)
{
    return GR_RENDER_STACK_SIZE;
}
