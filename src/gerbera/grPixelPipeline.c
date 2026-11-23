/* grPixelPipeline.c - the pixel-pipe dispatch (port of grPixelPipeline.h).
 *
 * The compiled program (state->pixelPipelineCode, produced by the hc
 * shader compiler) consumes the pixel-pipe record at
 * renderStack+14644: the row pointers (+168..+176), scanline state
 * (+180..+188) and the gradient arrays set up by grSetupTriangleGradients.
 */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "gerbera/grRGBA.h"
#include "gerbera/hc/hcCompiler.h"
#include "hg/hgFill.h"
#include "hg/hgInt32.h"

#include <stdint.h>
#include <string.h>

/* g_mutableParamsMask (VA 0x4785B4): bitmask of mutable pixel-pipe stack
 * parameters among the 78 words (params 0..77). */
static unsigned char g_mutableParamsMask[10] = {
    0xFEu, 0xFFu, 0xFEu, 0xFFu, 0x00u, 0xFCu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
};

/* stack canary words kept by the decompile's locals (grSmall.c 86) */
#define GR_STACK_CHECK_MAGIC0 (-559038737)
#define GR_STACK_CHECK_MAGIC1 (-1073628482)

/* --- grExecutePixelPipeline (grPixelPipeline.h 153; DLL 24757-24775) --------- */

void grExecutePixelPipeline(Gerbera_s *state)
{
    grPixelPipelineCode_fn pixelPipelineCode = state->pixelPipelineCode;

    if (pixelPipelineCode == NULL &&
        grAssert("pixelPipelineCode", "grPixelPipeline.h", 153) != 0)
        grDie(0);
    /* the decompile reaches this through renderStack->pixelPipelineStack -
     * the pixel-pipe pointer living at renderStack+14644 (the same address
     * passed to the program); state->pixelPipeline is that pointer */
    if (state->pixelPipeline == NULL &&
        grAssert("state->renderStack->pixelPipelineStack", "grPixelPipeline.h", 154) != 0)
        grDie(0);
    pixelPipelineCode(state->pixelPipeline);
}

/* --- grIsGradientUsed (grPixelPipeline.inl 702; DLL 36949-36959) --------------
 * Reads gradient g's bit out of the HC compiler's per-parameter mask (the
 * mask indexes parameters, gradients start at bit 48). */

int grIsGradientUsed(unsigned char *paramMask, int g)
{
    if (paramMask == NULL &&
        grAssert("paramMask", "grPixelPipeline.inl", 702) != 0)
        grDie(0);
    return (paramMask[(g + 48) >> 3] >> ((g + 48) & 7)) & 1;
}

/* --- signature bit-count tables (data.fragments.c 3222/3281) ------------------
 * Per-BYTE significant-bit counts of the 144-byte pipeline signature: 56
 * main bytes (descriptor + scalars) then texture[2][44]. High bits past the
 * count are padding and get squeezed out by grCompressSignature. */

static const unsigned char s_mainBitCounts[56] =
{
    5, 5, 5, 5, 4, 4, 4, 4, 4, 3, 3, 3, 3, 0, 0, 0,
    1, 4, 1, 1, 3, 1, 3, 1, 1, 1, 4, 1, 4, 4, 1, 1,
    1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char s_textureBitCounts[44] =
{
    5, 5, 5, 5, 4, 4, 4, 4, 4, 3, 3, 3, 3, 2, 0, 0,
    1, 1, 3, 1, 1, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 2, 2, 1, 1, 0, 0
};

/* --- grCompressSignature (grPixelPipeline.inl 845; DLL 36962-37109) -----------
 * Packs the 144-byte signature into the 40-byte cache key, squeezing each
 * byte down to its bit count and packing MSB-first into 32-bit words (a
 * byte may spill across a word boundary). */

void grCompressSignature(_grCacheSignature *compressed,
                         _grSignature *uncompressed)
{
    const unsigned char *src = (const unsigned char *)uncompressed;
    unsigned int *dst = (unsigned int *)compressed->bytes;
    unsigned int value = 0;
    int numFreeBits = 32;
    int i;

    for (i = 0; i < 56; ++i)
    {
        unsigned int srcValue = *src++;
        int srcSize = s_mainBitCounts[i];
        int includeBits = hgMin32(srcSize, numFreeBits);
        int excludeBits = srcSize - includeBits;

        if (((int)(~0u << srcSize) & (int)srcValue) != 0 &&
            grAssert("!((0xFFFFFFFF << srcSize) & srcValue)",
                     "grPixelPipeline.inl", 845) != 0)
            grDie(0);
        if (((value << includeBits) & (srcValue >> excludeBits)) != 0 &&
            grAssert("!((value << includeBits) & (srcValue >> excludeBits))",
                     "grPixelPipeline.inl", 846) != 0)
            grDie(0);
        value = (value << includeBits) + (srcValue >> excludeBits);
        numFreeBits -= includeBits;
        if (hgInRange32(numFreeBits, 0, 32) == 0 &&
            grAssert("hgInRange32(numFreeBits, 0, 32)",
                     "grPixelPipeline.inl", 850) != 0)
            grDie(0);
        if (numFreeBits == 0)
        {
            *dst++ = value;
            value = 0;
            numFreeBits = 32;
        }
        if (excludeBits != 0)
        {
            if ((numFreeBits != 32 || value != 0) &&
                grAssert("numFreeBits == 32 && !value",
                         "grPixelPipeline.inl", 860) != 0)
                grDie(0);
            value = srcValue & ((1 << excludeBits) - 1);
            numFreeBits -= excludeBits;
        }
    }
    for (i = 0; i < 2; ++i)
    {
        if ((const unsigned char *)src !=
            (const unsigned char *)&uncompressed->texture[i] &&
            grAssert("src == (HGuint8*)&uncompressed->texture[texIndex]",
                     "grPixelPipeline.inl", 872) != 0)
            grDie(0);
        {
            int j;

            for (j = 0; j < 44; ++j)
            {
                unsigned int srcValue = *src++;
                int srcSize = s_textureBitCounts[j];
                int includeBits = hgMin32(srcSize, numFreeBits);
                int excludeBits = srcSize - includeBits;

                if (((int)(~0u << srcSize) & (int)srcValue) != 0 &&
                    grAssert("!((0xFFFFFFFF << srcSize) & srcValue)",
                             "grPixelPipeline.inl", 881) != 0)
                    grDie(0);
                if (((value << includeBits) & (srcValue >> excludeBits)) != 0 &&
                    grAssert("!((value << includeBits) & (srcValue >> excludeBits))",
                             "grPixelPipeline.inl", 882) != 0)
                    grDie(0);
                value = (value << includeBits) + (srcValue >> excludeBits);
                numFreeBits -= includeBits;
                if (hgInRange32(numFreeBits, 0, 32) == 0 &&
                    grAssert("hgInRange32(numFreeBits, 0, 32)",
                             "grPixelPipeline.inl", 886) != 0)
                    grDie(0);
                if (numFreeBits == 0)
                {
                    *dst++ = value;
                    value = 0;
                    numFreeBits = 32;
                }
                if (excludeBits != 0)
                {
                    if ((numFreeBits != 32 || value != 0) &&
                        grAssert("numFreeBits == 32 && !value",
                                 "grPixelPipeline.inl", 896) != 0)
                        grDie(0);
                    value = srcValue & ((1 << excludeBits) - 1);
                    numFreeBits -= excludeBits;
                }
            }
        }
    }
    if ((const unsigned char *)src !=
        (const unsigned char *)(uncompressed + 1) &&
        grAssert("src == (HGuint8*)uncompressed + sizeof(grSignature)",
                 "grPixelPipeline.inl", 903) != 0)
        grDie(0);
    if (numFreeBits != 32)
        *dst++ = value;
    if ((unsigned char *)dst !=
        compressed->bytes + sizeof(compressed->bytes) &&
        grAssert("dst == &compressed->data[(GR_CACHE_SIGNATURE_SIZE+3)/4]",
                 "grPixelPipeline.inl", 909) != 0)
        grDie(0);
}

/* --- grEvaluatePixelPipeline (grPixelPipeline.inl 89; DLL 36304-36946) ---------
 * Builds the 144-byte pipeline signature from the current state: the
 * draw-surface descriptor, blend/logic/alpha/depth/stencil/fog setup and
 * the per-texture-unit env decode. The decompile uses an 8-descriptor
 * scratch array (memset 0xCC) whose fields are reused as flags/pointers
 * across phases; the port keeps the value fields and replaces the spilled
 * pointers with typed locals. All raw record offsets are kept. */

void grEvaluatePixelPipeline(Gerbera_s *state, _grSignature *signature)
{
    _grSurfaceDescriptor combinedMask[8];
    _grDrawSurface *drawSurface;

    memset(combinedMask, 0xCC, sizeof(combinedMask));
    drawSurface = &state->drawSurface;
    *(int *)&combinedMask[6].colorMask =
        (unsigned char)(state->colorMask & drawSurface->colorMask);
    *(int *)combinedMask[6].rgbaOffsets = 0;
    *(int *)combinedMask[6].rgbaSizes = 0;
    *(int *)&combinedMask[5].pixelFormat = 1;
    *(int *)&combinedMask[5].colorMask =
        (combinedMask[6].colorMask & 7) != 0;
    *(int *)combinedMask[5].rgbaSizes = (combinedMask[6].colorMask & 8) != 0;
    if ((state == NULL || signature == NULL) &&
        grAssert("state && signature", "grPixelPipeline.inl", 97) != 0)
        grDie(0);
    hgFill8((unsigned char *)&signature->descriptor, 0, 144);
    signature->isValid = 1;
    signature->dither = (unsigned char)state->useDithering;
    *(int *)&combinedMask[5].pixelFormat =
        (state->useDithering == 0) & *(int *)&combinedMask[5].pixelFormat;
    if (grTestCapability(state, GR_CAPABILITY_BLEND) != 0 &&
        (combinedMask[6].colorMask != 0u ||
         combinedMask[6].colorBytes != 0u ||
         combinedMask[6].depthBytes != 0u ||
         combinedMask[6].stencilBytes != 0u) &&
        (grGetSrcBlendFactor(state) != GR_BLENDFACTOR_ONE ||
         grGetDstBlendFactor(state) != GR_BLENDFACTOR_ZERO))
    {
        *(int *)combinedMask[6].rgbaOffsets = 1;
    }
    if (grTestCapability(state, GR_CAPABILITY_COLOR_LOGIC_OP) != 0)
    {
        *(int *)combinedMask[6].rgbaOffsets = 0;
        *(int *)combinedMask[6].rgbaSizes = state->logicOp != 3;
    }
    if (*(int *)combinedMask[6].rgbaOffsets != 0 ||
        *(int *)combinedMask[6].rgbaSizes != 0)
    {
        *(int *)&combinedMask[5].pixelFormat = 0;
    }
    if (grTestCapability(state, GR_CAPABILITY_DEPTH_TEST) != 0 &&
        drawSurface->buffers[1].data != 0 &&
        grTestCapability(state, GR_CAPABILITY_DEPTH_TEST) != 0 &&
        state->depthMask != 0)
    {
        int lineOffset = grTestCapability(state,
                                          GR_CAPABILITY_DEPTH_OFFSET_LINE);
        int anyOffset = grTestCapability(state,
                                         GR_CAPABILITY_POLYGON_OFFSET_FILL) |
                        lineOffset;

        if ((grTestCapability(state,
                              GR_CAPABILITY_DEPTH_OFFSET_POINT) |
             anyOffset) != 0)
        {
            *(int *)combinedMask[4].rgbaSizes =
                hgSign32(state->depthOffsetFactor);
            *(int *)combinedMask[4].rgbaOffsets =
                hgSign32(state->depthOffsetUnits);
            signature->depthClampMin =
                (*(int *)combinedMask[4].rgbaSizes != -1)
                    ? (unsigned char)(*(int *)combinedMask[4].rgbaOffsets == -1)
                    : 1;
            signature->depthClampMax =
                (*(int *)combinedMask[4].rgbaSizes != 1)
                    ? (unsigned char)(*(int *)combinedMask[4].rgbaOffsets == -1)
                    : 1;
        }
    }
    signature->logicOpEnabled = (unsigned char)*(int *)combinedMask[6].rgbaSizes;
    if (*(int *)combinedMask[6].rgbaSizes != 0)
        signature->logicOp = (unsigned char)grGetLogicOp(state);
    else
        signature->logicOp = 0;
    signature->blendEnabled =
        (unsigned char)*(int *)combinedMask[6].rgbaOffsets;
    if (*(int *)combinedMask[6].rgbaOffsets != 0)
    {
        signature->srcFactor = (unsigned char)grGetSrcBlendFactor(state);
        signature->dstFactor = (unsigned char)grGetDstBlendFactor(state);
    }
    signature->colorMask = (unsigned char)state->colorMask;
    *(int *)signature->descriptor.rgbaOffsets =
        *(int *)drawSurface->rgbaOffsets;
    *(int *)signature->descriptor.rgbaSizes =
        *(int *)drawSurface->rgbaSizes;
    *(int *)&signature->descriptor.colorMask =
        *(int *)&drawSurface->colorMask;
    *(int *)&signature->descriptor.pixelFormat =
        *(int *)&drawSurface->pixelFormat;
    if (drawSurface->buffers[0].data != 0 &&
        (signature->colorMask & signature->descriptor.colorMask) != 0 &&
        (signature->blendEnabled == 0 || signature->srcFactor != 0 ||
         signature->dstFactor != 1))
    {
        signature->writeColor = 1;
    }
    if (grTestCapability(state, GR_CAPABILITY_FOG) != 0)
    {
        *(int *)&combinedMask[3].pixelFormat =
            state->colorMask & drawSurface->colorMask & 7;
        if (*(int *)&combinedMask[3].pixelFormat != 0)
        {
            *(unsigned int *)&combinedMask[3].colorMask = state->fogColor;
            signature->fogEnabled = 1;
            *(int *)combinedMask[3].rgbaSizes =
                *(int *)&combinedMask[3].colorMask;
            if (grMaskedColorsEqual(*(unsigned int *)&combinedMask[3].colorMask,
                                    *(unsigned int *)&combinedMask[3].pixelFormat) != 0)
            {
                int ndx = 31 - hgClz32(
                    *(unsigned int *)&combinedMask[3].pixelFormat);
                if ((hgInRange32(ndx, 0, 3) == 0) &&
                    grAssert("hgInRange32(ndx, 0, 3)",
                             "grPixelPipeline.inl", 197) != 0)
                    grDie(0);
                signature->monochromeFog = 1;
                *(int *)combinedMask[3].rgbaSizes =
                    grUnpackRGBA(*(unsigned int *)&combinedMask[3].colorMask,
                                 ndx);
            }
            state->packedFogColor = *(int *)combinedMask[3].rgbaSizes;
            *(int *)&combinedMask[5].pixelFormat = 0;
        }
    }
    if (grTestCapability(state, GR_CAPABILITY_ALPHA_TEST) != 0 &&
        grGetAlphaFunc(state) != GR_FUNC_ALWAYS)
    {
        *(int *)&combinedMask[2].pixelFormat = grGetAlphaFunc(state);
        signature->alphaTest = 1;
        signature->alphaFunc = (unsigned char)*(int *)&combinedMask[2].pixelFormat;
    }
    if (grTestCapability(state, GR_CAPABILITY_DEPTH_TEST) != 0 &&
        drawSurface->buffers[1].data != 0 &&
        (state->depthFunc != 7 || state->depthMask != 0))
    {
        signature->depthTest = 1;
        signature->depthFunc = (unsigned char)state->depthFunc;
        signature->depthMask = (unsigned char)state->depthMask;
        if (state->viewportScale.normal[2] == 0 &&
            (state->depthFunc != 7 || state->depthMask != 0))
        {
            signature->constantDepth = 1;
        }
    }
    combinedMask[2].colorMask = 0u;
    combinedMask[2].colorBytes = 0u;
    combinedMask[2].depthBytes = 0u;
    combinedMask[2].stencilBytes = 0u;
    *(int *)combinedMask[2].rgbaSizes = grGetStencilFunc(state);
    *(int *)combinedMask[2].rgbaOffsets = grGetStencilFail(state);
    *(int *)&combinedMask[1].pixelFormat = grGetStencilZFail(state);
    *(int *)&combinedMask[1].colorMask = grGetStencilZPass(state);
    if (grTestCapability(state, GR_CAPABILITY_STENCIL_TEST) != 0 &&
        drawSurface->buffers[2].data != 0)
    {
        combinedMask[2].colorMask = 1u;
        combinedMask[2].colorBytes = 0u;
        combinedMask[2].depthBytes = 0u;
        combinedMask[2].stencilBytes = 0u;
        if (*(int *)combinedMask[2].rgbaSizes == 7)
        {
            *(int *)combinedMask[2].rgbaOffsets = 0;
            if (signature->depthTest != 0)
            {
                if (signature->depthFunc == 7)
                {
                    *(int *)&combinedMask[1].pixelFormat = 0;
                }
                else if (signature->depthFunc == 0)
                {
                    combinedMask[1].colorMask = 0u;
                    combinedMask[1].colorBytes = 0u;
                    combinedMask[1].depthBytes = 0u;
                    combinedMask[1].stencilBytes = 0u;
                }
            }
            else
            {
                *(int *)&combinedMask[1].pixelFormat = 0;
            }
            if (*(int *)&combinedMask[1].pixelFormat == 0 &&
                combinedMask[1].colorMask == 0u &&
                combinedMask[1].colorBytes == 0u &&
                combinedMask[1].depthBytes == 0u &&
                combinedMask[1].stencilBytes == 0u)
            {
                combinedMask[2].colorMask = 0u;
                combinedMask[2].colorBytes = 0u;
                combinedMask[2].depthBytes = 0u;
                combinedMask[2].stencilBytes = 0u;
            }
        }
        else if (*(int *)combinedMask[2].rgbaSizes == 0)
        {
            combinedMask[1].colorMask = 0u;
            combinedMask[1].colorBytes = 0u;
            combinedMask[1].depthBytes = 0u;
            combinedMask[1].stencilBytes = 0u;
            *(int *)&combinedMask[1].pixelFormat = 0;
        }
    }
    signature->stencilTest = (unsigned char)combinedMask[2].colorMask;
    if (combinedMask[2].colorMask != 0u ||
        combinedMask[2].colorBytes != 0u ||
        combinedMask[2].depthBytes != 0u ||
        combinedMask[2].stencilBytes != 0u)
    {
        signature->stencilFunc =
            (unsigned char)*(int *)combinedMask[2].rgbaSizes;
        signature->stencilFail =
            (unsigned char)*(int *)combinedMask[2].rgbaOffsets;
        signature->stencilZFail =
            (unsigned char)*(int *)&combinedMask[1].pixelFormat;
        signature->stencilZPass =
            (unsigned char)*(int *)&combinedMask[1].colorMask;
        signature->stencilTestMaskFull =
            (unsigned char)grIsStencilMaskFull(state, state->stencilTestMask);
        signature->stencilWriteMaskFull =
            (unsigned char)grIsStencilMaskFull(state, state->stencilWriteMask);
        if (*(int *)&combinedMask[1].pixelFormat ==
            *(int *)&combinedMask[1].colorMask)
        {
            if (*(int *)combinedMask[2].rgbaOffsets ==
                *(int *)&combinedMask[1].pixelFormat)
                signature->stencilAllOpsEqual = 1;
            else
                signature->stencilZOpsEqual = 1;
        }
    }
    *(int *)combinedMask[5].rgbaOffsets = signature->flatShading;
    *(int *)&combinedMask[4].pixelFormat = 1;
    combinedMask[4].colorMask = 1u;
    combinedMask[4].colorBytes = 0u;
    combinedMask[4].depthBytes = 0u;
    combinedMask[4].stencilBytes = 0u;
    while (*(int *)&combinedMask[4].colorMask >= 0)
    {
        int unit = *(int *)&combinedMask[4].colorMask;
        _grTextureUnitState *unitRec = &state->textureUnit[2 * unit];
        _grTexture *texObj = (_grTexture *)unitRec->texture;

        if (state->useTexUnit[unit] != 0)
        {
            unsigned char *rec = signature->texture[unit];
            int pf;

            grrGetTextureFormatDescriptor(&combinedMask[7], texObj->texFormat);
            *(int *)&combinedMask[0].colorMask =
                (int)(unsigned char)combinedMask[7].pixelFormat;
            rec[16] = 1;
            rec[17] = (unsigned char)((texObj->flags183 & 0x10) != 0);
            rec[19] = (unsigned char)((texObj->flags207 & 1) != 0);
            rec[20] = (unsigned char)((texObj->flags206 & 1) != 0);
            rec[21] = texObj->coordFormat[0];
            rec[22] = texObj->coordFormat[1];
            *(int *)&rec[0] = *(int *)combinedMask[7].rgbaOffsets;
            *(int *)&rec[4] = *(int *)combinedMask[7].rgbaSizes;
            *(int *)&rec[8] = *(int *)&combinedMask[7].colorMask;
            *(int *)&rec[12] = *(int *)&combinedMask[7].pixelFormat;
            rec[38] = 0;
            rec[39] = 0;
            rec[26] = 0;
            rec[30] = 0;
            rec[34] = 0;
            rec[27] = 2;
            rec[31] = 2;
            rec[35] = 2;
            rec[28] = 0;
            rec[32] = 0;
            rec[29] = 0;
            rec[33] = 0;
            rec[36] = 0xFF;
            rec[37] = 0xFF;
            rec[24] = 0xFF;
            rec[25] = 0xFF;
            pf = *(int *)&combinedMask[0].colorMask;
            switch (unitRec->envMode)
            {
                case 0:
                    if (pf != 0)
                    {
                        if (pf == 1 || pf == 4)
                        {
                            rec[36] = 0;
                            rec[37] = 1;
                        }
                        else
                        {
                            if ((pf != 2 && pf != 3 && pf != 5) &&
                                grAssert(
                                    "pixelFormat == GR_PIXELFORMAT_LUMINANCE_ALPHA || pixelFormat == GR_PIXELFORMAT_INTENSITY || pixelFormat == GR_PIXELFORMAT_RGBA",
                                    "grPixelPipeline.inl", 408) != 0)
                                grDie(0);
                            rec[36] = 0;
                            rec[37] = 0;
                        }
                    }
                    else
                    {
                        rec[36] = 1;
                        rec[37] = 0;
                    }
                    rec[24] = 3;
                    rec[28] = 0;
                    rec[25] = 3;
                    rec[29] = 0;
                    break;
                case 1:
                    if (pf == 5)
                    {
                        rec[24] = 0;
                        rec[28] = 3;
                        rec[32] = 0;
                        rec[34] = 2;
                        rec[36] = 4;
                    }
                    else
                    {
                        rec[24] = 0;
                        rec[36] = 1;
                    }
                    rec[25] = 3;
                    rec[37] = 1;
                    break;
                case 2:
                    if (pf != 0)
                    {
                        if (pf == 3)
                        {
                            rec[24] = 1;
                            rec[28] = 3;
                            rec[32] = 0;
                            rec[36] = 4;
                            rec[25] = 1;
                            rec[29] = 3;
                            rec[33] = 0;
                            rec[37] = 4;
                        }
                        else if (pf == 1 || pf == 4)
                        {
                            rec[24] = 1;
                            rec[28] = 3;
                            rec[32] = 0;
                            rec[36] = 4;
                            rec[25] = 3;
                            rec[37] = 1;
                        }
                        else
                        {
                            if ((pf != 2 && pf != 5) &&
                                grAssert(
                                    "pixelFormat == GR_PIXELFORMAT_LUMINANCE_ALPHA || pixelFormat == GR_PIXELFORMAT_RGBA",
                                    "grPixelPipeline.inl", 463) != 0)
                                grDie(0);
                            rec[24] = 1;
                            rec[28] = 3;
                            rec[32] = 0;
                            rec[36] = 4;
                            rec[25] = 3;
                            rec[29] = 0;
                            rec[37] = 0;
                        }
                    }
                    else
                    {
                        rec[24] = 3;
                        rec[36] = 1;
                        rec[25] = 3;
                        rec[29] = 0;
                        rec[37] = 0;
                    }
                    break;
                case 3:
                    if (pf != 0)
                    {
                        if (pf == 1 || pf == 4)
                        {
                            rec[24] = 0;
                            rec[25] = 3;
                        }
                        else
                        {
                            if ((pf != 2 && pf != 3 && pf != 5) &&
                                grAssert(
                                    "pixelFormat == GR_PIXELFORMAT_LUMINANCE_ALPHA || pixelFormat == GR_PIXELFORMAT_INTENSITY || pixelFormat == GR_PIXELFORMAT_RGBA",
                                    "grPixelPipeline.inl", 383) != 0)
                                grDie(0);
                            rec[24] = 0;
                            rec[25] = 0;
                        }
                    }
                    else
                    {
                        rec[24] = 3;
                        rec[25] = 0;
                    }
                    rec[36] = 1;
                    rec[37] = 1;
                    break;
                case 4:
                    if (pf != 0)
                    {
                        if (pf == 3)
                        {
                            rec[24] = 3;
                            rec[28] = 0;
                            rec[36] = 2;
                            rec[25] = 3;
                            rec[29] = 0;
                            rec[37] = 2;
                        }
                        else if (pf == 1 || pf == 4)
                        {
                            rec[24] = 3;
                            rec[28] = 0;
                            rec[36] = 2;
                            rec[25] = 3;
                            rec[37] = 1;
                        }
                        else
                        {
                            if ((pf != 2 && pf != 5) &&
                                grAssert(
                                    "pixelFormat == GR_PIXELFORMAT_LUMINANCE_ALPHA || pixelFormat == GR_PIXELFORMAT_RGBA",
                                    "grPixelPipeline.inl", 515) != 0)
                                grDie(0);
                            rec[24] = 3;
                            rec[28] = 0;
                            rec[36] = 2;
                            rec[25] = 3;
                            rec[29] = 0;
                            rec[37] = 0;
                        }
                    }
                    else
                    {
                        rec[24] = 3;
                        rec[36] = 1;
                        rec[25] = 3;
                        rec[29] = 0;
                        rec[37] = 0;
                    }
                    break;
                case 5:
                    rec[36] = unitRec->envFuncRGB;
                    rec[37] = unitRec->envFuncAlpha;
                    rec[38] = (unsigned char)(31 - hgClz32(
                        (unsigned int)unitRec->envScaleRGB));
                    rec[39] = (unsigned char)(31 - hgClz32(
                        (unsigned int)unitRec->envScaleAlpha));
                    rec[24] = unitRec->envSrcRGB[0];
                    rec[26] = unitRec->envOpRGB[0];
                    if (rec[36] != 1)
                    {
                        rec[28] = unitRec->envSrcRGB[1];
                        rec[30] = unitRec->envOpRGB[1];
                        if (unitRec->envFuncRGB == 4)
                        {
                            rec[32] = unitRec->envSrcRGB[2];
                            rec[34] = unitRec->envOpRGB[2];
                        }
                    }
                    rec[25] = unitRec->envSrcAlpha[0];
                    rec[27] = unitRec->envOpAlpha[0];
                    if (rec[37] != 1)
                    {
                        rec[29] = unitRec->envSrcAlpha[1];
                        rec[31] = unitRec->envOpAlpha[1];
                        if (rec[37] == 4)
                        {
                            rec[33] = unitRec->envSrcAlpha[2];
                            rec[35] = unitRec->envOpAlpha[2];
                        }
                    }
                    break;
                default:
                    if (grAssert("HG_FALSE",
                                 "grPixelPipeline.inl", 605) != 0)
                        grDie(0);
                    break;
            }
            if ((hgInRange32(rec[24], 0, 3) == 0) &&
                grAssert("hgInRange32(tex->rgbSource0, 0, GR_TEXENVCOMBSRC_MAX-1)",
                         "grPixelPipeline.inl", 609) != 0)
                grDie(0);
            if ((hgInRange32(rec[25], 0, 3) == 0) &&
                grAssert("hgInRange32(tex->alphaSource0, 0, GR_TEXENVCOMBSRC_MAX-1)",
                         "grPixelPipeline.inl", 610) != 0)
                grDie(0);
            if ((hgInRange32(rec[36], 0, 7) == 0) &&
                grAssert("hgInRange32(tex->rgbFunction, 0, GR_TEXENVCOMBFUNC_MAX-1)",
                         "grPixelPipeline.inl", 611) != 0)
                grDie(0);
            if ((hgInRange32(rec[37], 0, 7) == 0) &&
                grAssert("hgInRange32(tex->alphaFunction, 0, GR_TEXENVCOMBFUNC_MAX-1)",
                         "grPixelPipeline.inl", 612) != 0)
                grDie(0);
            rec[40] = 1;
            rec[41] = 1;
            if (rec[36] == 1 && rec[24] == 0 && rec[26] == 0)
            {
                combinedMask[5].colorMask = 0u;
                combinedMask[5].colorBytes = 0u;
                combinedMask[5].depthBytes = 0u;
                combinedMask[5].stencilBytes = 0u;
            }
            if (rec[37] == 1 && rec[25] == 0 && rec[27] == 2)
            {
                *(int *)combinedMask[5].rgbaSizes = 0;
            }
            if (*(int *)&combinedMask[5].pixelFormat != 0 &&
                rec[36] == 1 && rec[24] == 0 && rec[26] == 0 &&
                ((combinedMask[6].colorMask & 8) == 0 ||
                 (rec[37] == 1 && rec[25] == 0 && rec[27] == 2)) &&
                rec[19] == 0 && rec[20] == 0 &&
                (combinedMask[7].pixelFormat == 4 ||
                 combinedMask[7].pixelFormat == 5) &&
                grMaskedDescriptorsEqual(
                    *(unsigned int *)&combinedMask[6].colorMask,
                    (_grSurfaceDescriptor *)&state->drawSurface,
                    &combinedMask[7]) != 0)
            {
                if (!(combinedMask[5].colorMask == 0u &&
                      combinedMask[5].colorBytes == 0u &&
                      combinedMask[5].depthBytes == 0u &&
                      combinedMask[5].stencilBytes == 0u &&
                      *(int *)combinedMask[5].rgbaSizes == 0) &&
                    grAssert("!vertexRGBUsed && !vertexAlphaUsed",
                             "grPixelPipeline.inl", 646) != 0)
                    grDie(0);
                rec[23] = 1;
                *(int *)&combinedMask[4].pixelFormat = 0;
            }
            *(int *)&combinedMask[5].pixelFormat = 0;
            *(int *)combinedMask[5].rgbaOffsets = 0;
        }
        --*(int *)&combinedMask[4].colorMask;
    }
    if (grTestCapability(state, GR_CAPABILITY_SMOOTH_SHADING) == 0 ||
        (signature->alphaTest == 0 &&
         combinedMask[5].colorMask == 0u &&
         combinedMask[5].colorBytes == 0u &&
         combinedMask[5].depthBytes == 0u &&
         combinedMask[5].stencilBytes == 0u &&
         *(int *)combinedMask[5].rgbaSizes == 0))
    {
        signature->flatShading = 1;
    }
    {
        int v28 = 0;
        int v29 = 0;

        if (signature->blendEnabled == 0)
            v29 = signature->fogEnabled == 0;
        if (v29 && signature->dither == 0)
            v28 = 1;
        *(int *)combinedMask[5].rgbaOffsets &= v28;
        *(int *)&combinedMask[4].pixelFormat =
            (*(int *)combinedMask[5].rgbaOffsets == 0) &
            *(int *)&combinedMask[4].pixelFormat;
        signature->constantWriteColor =
            (unsigned char)*(int *)combinedMask[5].rgbaOffsets;
        signature->perPixelPack =
            (unsigned char)*(int *)&combinedMask[4].pixelFormat;
    }
    *(int *)combinedMask[0].rgbaSizes = signature->descriptor.colorMask;
    *(int *)combinedMask[0].rgbaOffsets =
        (unsigned char)(combinedMask[0].rgbaSizes[0] & signature->colorMask);
    {
        int v30 = 0;

        if (*(int *)combinedMask[0].rgbaOffsets != 0)
            v30 = *(int *)combinedMask[0].rgbaOffsets !=
                  *(int *)combinedMask[0].rgbaSizes;
        signature->applyColorWriteMask = (unsigned char)v30;
    }
    signature->screenRectGradients =
        (unsigned char)grGetRectGradients(state);
}

/* --- grEnsureValidPixelPipeline (grPixelPipeline.inl 1080; DLL 37112-37279) ----
 * Evaluates + compresses the signature, looks the program up in the
 * pipeline cache and, on a hit, publishes the entry's gradient mask. On a
 * miss the DLL grows the scratch memory, runs the HC program generator
 * (grExecuteProgram) and the bytecode compiler (HC_compile, backend from
 * grGetBackend) and inserts the result; that half lands with the hc-module
 * session. Until then a miss takes the decompile's compile-failure exit
 * (compilationFailed = 1, early return). */

void grEnsureValidPixelPipeline(Gerbera_s *state)
{
    _grSignature signature;
    _grCacheSignature compressed;
    unsigned char paramMask[16];
    void *entry;
    unsigned int gradientMask = 0;
    unsigned int magic0 = GR_STACK_CHECK_MAGIC0;
    unsigned int magic1 = GR_STACK_CHECK_MAGIC1;

    grEvaluatePixelPipeline(state, &signature);
    grCompressSignature(&compressed, &signature);
    entry = grCacheFind(state->pipelineCache, &compressed, &gradientMask);
    if (entry == NULL)
    {
        HCBackend_s *backend = (HCBackend_s *)grGetBackend();
        void *code = NULL;
        int lengthInBytes;
        int g;

        state->compilationFailed = 1;
        if (state->scratchMemory == NULL)
        {
            if (state->scratchMemorySize != 0 &&
                grAssert("!state->scratchMemorySize",
                         "grPixelPipeline.inl", 1099) != 0)
                grDie(0);
            grScratchGrowMemory(state);
            if (state->scratchMemory == NULL)
            {
                if ((magic0 != (unsigned int)GR_STACK_CHECK_MAGIC0 ||
                     magic1 != (unsigned int)GR_STACK_CHECK_MAGIC1) &&
                    grAssert(
                        "__grMemoryCheck[0]==GR_STACK_CHECK_MAGIC0 && __grMemoryCheck[1]==GR_STACK_CHECK_MAGIC1",
                        "grPixelPipeline.inl", 1105) != 0)
                    grDie(0);
                return;
            }
        }

        for (;;)
        {
            char *scratch = (char *)state->scratchMemory;
            int scratchSize = state->scratchMemorySize;
            HCInstr_s *hcTmpCode = (HCInstr_s *)scratch;
            int instrCount = grExecuteProgram(hcTmpCode, &signature,
                                              scratchSize >> 3);

            scratch += 8 * instrCount;
            scratchSize -= 8 * instrCount;
            if (!(scratch != NULL && scratchSize > 0 && instrCount > 0 && hcTmpCode != NULL) &&
                grAssert("scratch && scratchSize > 0 && instrCount > 0 && hcTmpCode",
                         "grPixelPipeline.inl", 1124) != 0)
                grDie(0);
            if (((uintptr_t)scratch & 3u) != 0 &&
                grAssert("!((HGuint32)scratch & (GR_MEMORY_ALIGNMENT-1))",
                         "grPixelPipeline.inl", 1125) != 0)
                grDie(0);

            lengthInBytes = HC_compile(
                scratch,
                scratchSize,
                (HCSetjmpConfig_s *)state->setjmpConfig,
                &code,
                hcTmpCode,
                backend,
                0x3FFu,
                paramMask,
                g_mutableParamsMask,
                78);
            if (lengthInBytes != -1)
            {
                state->compilationFailed = 0;
                break;
            }
            if (grScratchGrowMemory(state) == 0)
                break;
        }

        if (state->compilationFailed != 0)
        {
            if ((magic0 != (unsigned int)GR_STACK_CHECK_MAGIC0 ||
                 magic1 != (unsigned int)GR_STACK_CHECK_MAGIC1) &&
                grAssert(
                    "__grMemoryCheck[0]==GR_STACK_CHECK_MAGIC0 && __grMemoryCheck[1]==GR_STACK_CHECK_MAGIC1",
                    "grPixelPipeline.inl", 1149) != 0)
                grDie(0);
            return;
        }
        if (lengthInBytes <= 0 &&
            grAssert("lengthInBytes > 0", "grPixelPipeline.inl", 1153) != 0)
            grDie(0);

        gradientMask = 0;
        for (g = 0; g < 10; ++g)
            gradientMask += (unsigned int)(grIsGradientUsed(paramMask, g) << g);

        entry = grCacheInsert(state->pipelineCache, &compressed,
                              gradientMask, code, lengthInBytes);
    }
    {
        int index = 0;
        int g;

        for (g = 0; g < 10; ++g)
        {
            if (((1 << g) & (int)gradientMask) != 0)
                state->activeGradientIndex[index++] = g;
        }
        state->numActiveGradients = index;
        state->activeGradientMask = (int)gradientMask;
        state->pixelPipelineCode = (grPixelPipelineCode_fn)entry;
    }
    if ((magic0 != (unsigned int)GR_STACK_CHECK_MAGIC0 ||
         magic1 != (unsigned int)GR_STACK_CHECK_MAGIC1) &&
        grAssert(
            "__grMemoryCheck[0]==GR_STACK_CHECK_MAGIC0 && __grMemoryCheck[1]==GR_STACK_CHECK_MAGIC1",
            "grPixelPipeline.inl", 1196) != 0)
        grDie(0);
}

/* --- grCheckPixelPipeConsistency (grPixelPipeline.inl 1213; DLL 37282-37335) ---
 * Debug check: re-evaluates the signature and confirms the cache still
 * resolves to the active program (and its gradient mask). The decompile
 * keeps the stack canary pair past the locals. */

void grCheckPixelPipeConsistency(Gerbera_s *state)
{
    unsigned int magic0 = GR_STACK_CHECK_MAGIC0;
    unsigned int magic1 = GR_STACK_CHECK_MAGIC1;

    (void)magic0;
    (void)magic1;
    if (state->pixelPipelineDirty == 0)
    {
        _grSignature signature;
        _grCacheSignature compressed;
        unsigned int gradientMask = 0;
        void *code;

        grEvaluatePixelPipeline(state, &signature);
        grCompressSignature(&compressed, &signature);
        code = grCacheFind(state->pipelineCache, &compressed, &gradientMask);
        if ((void *)code != (void *)state->pixelPipelineCode &&
            grAssert("pixelPipelineCode == state->pixelPipelineCode",
                     "grPixelPipeline.inl", 1235) != 0)
            grDie(0);
        if (!((void *)code == NULL ||
              gradientMask == (unsigned int)state->activeGradientMask) &&
            grAssert("pixelPipelineCode ? (gradientMask == state->activeGradientMask) : HG_TRUE",
                     "grPixelPipeline.inl", 1236) != 0)
            grDie(0);
    }
    if ((magic0 != (unsigned int)GR_STACK_CHECK_MAGIC0 ||
         magic1 != (unsigned int)GR_STACK_CHECK_MAGIC1) &&
        grAssert(
            "__grMemoryCheck[0]==GR_STACK_CHECK_MAGIC0 && __grMemoryCheck[1]==GR_STACK_CHECK_MAGIC1",
            "grPixelPipeline.inl", 1239) != 0)
        grDie(0);
}
