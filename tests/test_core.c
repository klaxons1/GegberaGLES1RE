/* test_core.c - checks for the gerbera core foundational layer (grDefs.h). */
#include "gerbera/grDefs.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, failures = 0;

#define CHECK(cond)                          \
    do                                       \
    {                                        \
        ++checks;                            \
        if (!(cond))                         \
        {                                    \
            ++failures;                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                    \
    } while (0)

static void test_descriptors(void)
{
    _grSurfaceDescriptor a, b;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    grClearDescriptor(&a);
    CHECK(a.rgbaOffsets[0] == 0 && a.rgbaSizes[0] == 0);
    CHECK(a.colorMask == 0 && a.colorBytes == 0 && a.depthBytes == 0);
    CHECK(a.stencilBytes == 0 && a.pixelFormat == 0);
    grSetDescriptorSizes(&a, 5, 6, 5, 8);
    CHECK(grGetDescriptorSize(&a, 0) == 5);
    CHECK(grGetDescriptorSize(&a, 1) == 6);
    CHECK(grGetDescriptorSize(&a, 3) == 8);
    grSetDescriptorOffsets(&a, 0, 5, 11, 16);
    CHECK(grGetDescriptorOffset(&a, 0) == 0);
    CHECK(grGetDescriptorOffset(&a, 2) == 11);
    grClearDescriptor(&b);
    CHECK(grDescriptorsEqual(&a, &b) == 0);
    grSetDescriptorSizes(&b, 5, 6, 5, 8);
    grSetDescriptorOffsets(&b, 0, 5, 11, 16);
    CHECK(grDescriptorsEqual(&a, &b) == 1);
    b.colorMask = 0xF0;
    CHECK(grDescriptorsEqual(&a, &b) == 0);
    b.colorMask = 0;
    CHECK(grDescriptorsEqual(&a, &b) == 1);
    b.pixelFormat = 3;
    CHECK(grDescriptorsEqual(&a, &b) == 0);
}

static void test_rectangles(void)
{
    _grRect r;
    grSetRectangle(&r, 0, 100, 0, 200);
    CHECK(grIsRectangleEmpty(&r) == 0);
    grSetRectangle(&r, 10, 10, 0, 200);
    CHECK(grIsRectangleEmpty(&r) == 1);
    grSetRectangle(&r, 0, 100, 10, 10);
    CHECK(grIsRectangleEmpty(&r) == 1);
    CHECK(grIsAligned((void *)0x1000, 4) == 1);
    CHECK(grIsAligned((void *)0x1002, 4) == 0);
    CHECK(grIsAligned((void *)0x1003, 4) == 0);
    CHECK(grIsAligned((void *)0x1004, 4) == 1);
}

static void test_formats(void)
{
    /* data type size table verbatim */
    CHECK(grGetDataTypeSize(0) == 1);
    CHECK(grGetDataTypeSize(1) == 1);
    CHECK(grGetDataTypeSize(2) == 2);
    CHECK(grGetDataTypeSize(3) == 2);
    CHECK(grGetDataTypeSize(4) == 4);
    CHECK(grGetDataTypeSize(5) == 4);
    CHECK(grGetDataTypeSize(6) == 4);
    CHECK(grGetDataTypeSize(7) == 4);
    CHECK(grGetDataTypeSize(8) == 1);
    CHECK(grGetDataTypeSize(9) == 0);
    /* image format component counts: {1,3,4,1,2,3,4} */
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_ALPHA) == 1);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_RGB) == 3);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_RGBA) == 4);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_LUMINANCE) == 1);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_LUMINANCE_ALPHA) == 2);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_BGR) == 3);
    CHECK(grGetImageFormatNumComponents(6) == 4);
}

static void test_stack(void)
{
    _grStackAllocator st;
    static char buf[256];
    char *p, *q;

    grStackInit(&st, buf, 256);
    CHECK(grIsStackValid(&st) == 1);
    CHECK(grStackGetFreeSpace(&st) == 256);
    p = (char *)grStackAllocate(&st, 17);
    CHECK(p == buf);
    CHECK(grIsAligned(p, 4) == 1);
    CHECK(grStackGetFreeSpace(&st) == 256 - 20);   /* 17 rounded up to 20 */
    q = (char *)grStackAllocate(&st, 40);
    CHECK(q == buf + 20);
    CHECK(grStackGetFreeSpace(&st) == 256 - 60);
    grStackFree(&st, q);
    CHECK(grStackGetFreeSpace(&st) == 256 - 20);
    grStackFree(&st, p);
    CHECK(grStackGetFreeSpace(&st) == 256);
    CHECK(grStackGetPtr(&st, 128) == buf + 128);
}

static void test_rgba(void);
static void test_pack_color(void);
static void test_texture(void);
static void test_util_helpers(void);
static void test_state_cull(void);
static void test_state_viewport(void);
static void test_state_pixel(void);
static void test_state_material(void);

int main(void)
{
    test_descriptors();
    test_rectangles();
    test_formats();
    test_stack();
    test_rgba();
    test_pack_color();
    test_texture();
    test_util_helpers();
    test_state_cull();
    test_state_viewport();
    test_state_pixel();
    test_state_material();
    printf("core tests: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

/* --- grRGBA.h + grUtilSmall.inl ------------------------------------------ */
#include "gerbera/grRGBA.h"

static void test_rgba(void)
{
    unsigned int c = grSetRGBA(0x12, 0x34, 0x56, 0x78);
    CHECK(c == 0x78563412u);
    CHECK(grUnpackRGBA(c, GR_COLOR_R) == 0x12);
    CHECK(grUnpackRGBA(c, GR_COLOR_G) == 0x34);
    CHECK(grUnpackRGBA(c, GR_COLOR_B) == 0x56);
    CHECK(grUnpackRGBA(c, GR_COLOR_A) == 0x78);
    CHECK(grPackRGBA(0xAB, GR_COLOR_B) == 0x00AB0000u);
    CHECK(grPackRGBA(0xFF, GR_COLOR_A) == 0xFF000000u);
}

static void test_pack_color(void)
{
    _grSurfaceDescriptor desc;
    /* RGBA8888: standard byte layout */
    memset(&desc, 0, sizeof(desc));
    grSetDescriptorSizes(&desc, 8, 8, 8, 8);
    grSetDescriptorOffsets(&desc, 0, 8, 16, 24);
    CHECK(grIsValidDescriptor(&desc) == 1);
    CHECK(grPackColor(grSetRGBA(0x11, 0x22, 0x33, 0x44), &desc) == 0x44332211u);
    CHECK(grUnpackColor(0x44332211u, &desc) == grSetRGBA(0x11, 0x22, 0x33, 0x44));

    /* RGB565: r 5 @11, g 6 @5, b 5 @0, alpha none */
    grSetDescriptorSizes(&desc, 5, 6, 5, 0);
    grSetDescriptorOffsets(&desc, 11, 5, 0, 0);
    {
        unsigned int packed = grPackColor(grSetRGBA(0xFF, 0xFF, 0xFF, 0xFF), &desc);
        CHECK(packed == 0xFFFFu);
        {
            unsigned int up = grUnpackColor(packed, &desc);
            CHECK(grUnpackRGBA(up, GR_COLOR_R) == 0xFF);
            CHECK(grUnpackRGBA(up, GR_COLOR_G) == 0xFF);
            CHECK(grUnpackRGBA(up, GR_COLOR_B) == 0xFF);
            CHECK(grUnpackRGBA(up, GR_COLOR_A) == 0xFF);   /* size 0 alpha -> 255 */
        }
    }
    /* small channel expansions */
    grSetDescriptorSizes(&desc, 1, 2, 3, 8);
    grSetDescriptorOffsets(&desc, 0, 1, 3, 11);
    {
        unsigned int up = grUnpackColor(0x1u, &desc);   /* r bit set */
        CHECK(grUnpackRGBA(up, GR_COLOR_R) == 0xFF);
    }
    {
        unsigned int up = grUnpackColor(0x2u, &desc);   /* g = 01 */
        CHECK(grUnpackRGBA(up, GR_COLOR_G) == 0x55);   /* 01 -> 01010101 */
    }
    {
        unsigned int up = grUnpackColor(0x8u, &desc);   /* b = 001 */
        CHECK(grUnpackRGBA(up, GR_COLOR_B) == 36);     /* decompile: (c>>1)|32c|4c, c=1 -> 36 */
    }

    /* color write mask: only R and B enabled */
    grSetDescriptorSizes(&desc, 8, 8, 8, 8);
    grSetDescriptorOffsets(&desc, 0, 8, 16, 24);
    CHECK(grGetPackedColorWriteMask(&desc, 0x5u) == 0x00FF00FFu);
    CHECK(grGetPackedColorWriteMask(&desc, 0x2u) == 0x0000FF00u);
    CHECK(grGetPackedColorWriteMask(&desc, 0x0u) == 0u);

    /* masked descriptor equality */
    {
        _grSurfaceDescriptor a, b;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        grSetDescriptorSizes(&a, 5, 6, 5, 8);
        grSetDescriptorOffsets(&a, 11, 5, 0, 24);
        grSetDescriptorSizes(&b, 5, 6, 5, 8);
        grSetDescriptorOffsets(&b, 11, 5, 0, 24);
        CHECK(grMaskedDescriptorsEqual(0xFu, &a, &b) == 1);
        b.rgbaOffsets[1] = 6;
        CHECK(grMaskedDescriptorsEqual(0xFu, &a, &b) == 0);
        CHECK(grMaskedDescriptorsEqual(0xDu, &a, &b) == 1);  /* G bit not in mask */
        CHECK(grMaskedDescriptorsEqual(0x2u, &a, &b) == 0);
    }
}

/* --- grTexture.h ---------------------------------------------------------- */
#include "gerbera/grTexture.h"

static void test_texture(void)
{
    _grTexture tex;
    int dims[2];
    void *p;

    grInitTexture(&tex);
    CHECK(grIsValidTexture(&tex) == 1);
    CHECK(grGetTextureTarget(&tex) == GR_TEXTURETARGET_MAX);   /* unbound */
    CHECK(tex.filterMask[GR_FILTERTYPE_MIN] == 6);
    CHECK(tex.filterMask[GR_FILTERTYPE_MAG] == 1);
    CHECK(tex.numMipLevels == 1);
    CHECK(tex.mipmap[0].texFormat == GR_TEXTUREFORMAT_MAX);
    CHECK(tex.mipmap[10].log2Dim[0] == -1);
    CHECK(tex.genMipMaps == 0);
    CHECK(grIsPalettedTextureFormat(5) == 1);
    CHECK(grIsPalettedTextureFormat(14) == 1);
    CHECK(grIsPalettedTextureFormat(4) == 0);
    CHECK(grIsPalettedTextureFormat(15) == 0);
    CHECK(grGetTextureTargetDimensionality(GR_TEXTURETARGET_2D) == 2);

    /* bind (grUpdateTexture walks mips via dimensionality, which asserts 2D) */
    grSetTextureTarget(&tex, GR_TEXTURETARGET_2D);
    CHECK((tex.parmMask & 1u) != 0);
    /* set up a 4x4 level-0 image, no mipmaps */
    {
        static unsigned char pix[16 * 4];
        tex.mipmap[0].pixels = pix;
        tex.mipmap[0].log2Dim[0] = 2;
        tex.mipmap[0].log2Dim[1] = 2;
        tex.mipmap[0].texFormat = GR_TEXTUREFORMAT_RGBA;
        tex.mipmap[0].initialized = 1;
    }
    p = grGetTextureMipLevel(&tex, 0, dims);
    CHECK(p != NULL);
    CHECK(dims[0] == 2 && dims[1] == 2);
    CHECK(grGetTextureMipLevel(&tex, 1, dims) == NULL);
    CHECK(grGetTextureMipLevelFormat(&tex, 0) == GR_TEXTUREFORMAT_RGBA);
    CHECK(grTextureMipLevelInitialized(&tex, 0) == 1);

    /* update: MIN filter = 6 has USE_MIPMAPS; chain walk fails at level 1
     * (missing), so the final parmMask write is skipped entirely (faithful:
     * stays at the initial 2) */
    grUpdateTexture(&tex);
    CHECK(tex.parmMask == 2u);

    /* enable mipmaps + full chain */
    {
        static unsigned char pix2[8 * 4], pix3[4 * 4];
        tex.filterMask[GR_FILTERTYPE_MIN] = 6;
        tex.mipmap[1].pixels = pix2; tex.mipmap[1].log2Dim[0] = 1; tex.mipmap[1].log2Dim[1] = 1;
        tex.mipmap[1].texFormat = GR_TEXTUREFORMAT_RGBA;
        tex.mipmap[2].pixels = pix3; tex.mipmap[2].log2Dim[0] = 0; tex.mipmap[2].log2Dim[1] = 0;
        tex.mipmap[2].texFormat = GR_TEXTUREFORMAT_RGBA;
        grUpdateTexture(&tex);
        /* chain ends at level 2 (2x2 -> next would be 1x1 level 3 missing) */
        CHECK(tex.numMipLevels == 3);
        CHECK((tex.parmMask & 8u) != 0);           /* mipmap chain complete */
        CHECK((tex.parmMask & 4u) != 0);
    }
    /* setters */
    grSetTextureWrapMode(&tex, GR_TEXCOORD_S, 1);
    CHECK(grGetTextureWrapMode(&tex, GR_TEXCOORD_S) == 1);
    grSetTextureGenMipmaps(&tex, 1);
    CHECK(grGetTextureGenMipmaps(&tex) == 1);
    grSetTextureFilter(&tex, GR_FILTERTYPE_MAG, 1);
    CHECK(grGetTextureFilter(&tex, GR_FILTERTYPE_MAG) == 1);
    {
        _grRect r, r2;
        grSetRectangle(&r, 1, 2, 3, 4);
        grSetTextureCropRect(&tex, &r);
        grGetTextureCropRect(&tex, &r2);
        CHECK(r2.x0 == 1 && r2.x1 == 2 && r2.y0 == 3 && r2.y1 == 4);
    }
    CHECK(grGetTextureTarget(&tex) == GR_TEXTURETARGET_2D);
    CHECK((tex.parmMask & 1u) != 0);
}

/* --- state-free grUtil.h helpers ------------------------------------------ */
static void test_util_helpers(void)
{
    _grVector3i v;
    v.x = 10; v.y = 20; v.z = 30;
    CHECK(grVector3iGet(&v, 0) == 10);
    CHECK(grVector3iGet(&v, 2) == 30);
    CHECK(grUnpackIndex(grSetIndices(0x11, 0x22, 0x33), 0) == 0x11);
    CHECK(grUnpackIndex(grSetIndices(0x11, 0x22, 0x33), 2) == 0x33);
    CHECK(grSetIndices(255, 0, 128) == 0x8000FFu);
    /* masked colors equal: ref channel = highest set mask bit */
    {
        unsigned int c = grSetRGBA(0x44, 0x44, 0x44, 0x44);
        /* ref channel = highest set mask bit; only masked channels compare */
        CHECK(grMaskedColorsEqual(c, 0x1u) == 1);
        CHECK(grMaskedColorsEqual(grSetRGBA(0x44, 0x45, 0x44, 0x44), 0x1u) == 1);  /* G not in mask */
        CHECK(grMaskedColorsEqual(grSetRGBA(0x44, 0x45, 0x44, 0x44), 0x2u) == 1);  /* ref IS G */
        CHECK(grMaskedColorsEqual(grSetRGBA(0x44, 0x45, 0x44, 0x44), 0x5u) == 1);  /* ref = B; G ignored */
        CHECK(grMaskedColorsEqual(grSetRGBA(0x44, 0x45, 0x44, 0x44), 0x3u) == 0);  /* R != ref G */
        CHECK(grMaskedColorsEqual(grSetRGBA(0x44, 0x45, 0x44, 0x45), 0xFu) == 0);
    }
}

/* --- grState.h / grApiSmall.c ---------------------------------------------- */
#include "gerbera/grState.h"

static void test_state_cull(void)
{
    Gerbera_s st;
    memset(&st, 0, sizeof(st));

    grSetCullMode(&st, GR_WINDING_CW, 1u);
    CHECK(st.frontFace == GR_WINDING_CW);
    CHECK(st.cullMask == 1u);
    CHECK(st.cullNegative == 1);            /* cullMask bit0 set, CW */
    CHECK(st.lightingSideMask == 1);
    grSetCullMode(&st, GR_WINDING_CCW, 1u);
    CHECK(st.cullNegative == 0);            /* CCW flips the negative test */
    grSetCullMode(&st, GR_WINDING_CW, 0u);
    CHECK(st.cullNegative == 0);
    CHECK(grTestCapability(&st, GR_CAPABILITY_MAX - 1) == 0);
    grEnableCapability(&st, 10);
    CHECK(grTestCapability(&st, 10) == 1);
    grDisableCapability(&st, 10);
    CHECK(grTestCapability(&st, 10) == 0);
    grSetCapability(&st, GR_CAPABILITY_LIGHT_MODEL_TWO_SIDE, 1);
    CHECK(grGetCapability(&st, GR_CAPABILITY_LIGHT_MODEL_TWO_SIDE) == 1);
    CHECK(st.lightingSideMask == 3);        /* updated by grSetCapability */
    grSetCapability(&st, GR_CAPABILITY_LIGHT_MODEL_TWO_SIDE, 0);
    CHECK(st.lightingSideMask == 1);
    /* lights: caps 41..48 flip lightActiveMask bits */
    grSetCapability(&st, 44, 1);
    CHECK((st.lightActiveMask & (1 << 3)) != 0);
    CHECK((st.lightActiveDirtyMask & (1 << 3)) != 0);
    grSetCapability(&st, 44, 0);
    CHECK((st.lightActiveMask & (1 << 3)) == 0);
    CHECK(st.normalsToEyeDirty == 0);
    grSetCapability(&st, GR_CAPABILITY_RESCALE_NORMAL, 1);
    CHECK(st.normalsToEyeDirty == 1);
    CHECK(st.normalsToEyePalDirty[8] == 1 && st.normalsToEyePalDirty[0] == 1);
    /* color material marks all light colors dirty */
    grSetCapability(&st, GR_CAPABILITY_COLOR_MATERIAL, 1);
    CHECK(st.lightColorDirtyMask == 0xFFu);
}

static void test_state_viewport(void)
{
    Gerbera_s st;
    memset(&st, 0, sizeof(st));

    st.drawSurface.width = 320;
    st.drawSurface.height = 240;
    grSetViewport(&st, 10, 310, 20, 220);
    CHECK(st.viewportRectangle.x0 == 10 && st.viewportRectangle.x1 == 310);
    CHECK(st.viewportOffset.normal[0] == 16 * 10 + 8 * 300 + 8);
    CHECK(st.viewportOffset.normal[1] == 16 * 20 + 8 * 200 + 8);
    CHECK(st.viewportScale.normal[0] == (300 << 6));
    CHECK(st.viewportScale.normal[1] == (int)((unsigned)(20 - 220) * 64u));
    /* surface scissor = whole surface; render scissor = intersection */
    CHECK(st.surfaceScissor.x0 == 0 && st.surfaceScissor.x1 == 320);
    CHECK(st.renderScissor.x0 == 10 && st.renderScissor.x1 == 310);
    CHECK(st.renderScissor.y0 == 20 && st.renderScissor.y1 == 220);
    CHECK(st.scissorDirty == 0);
    CHECK(st.rejectAllStateDirty == 1);
    /* viewport outside the surface -> empty render scissor */
    st.rejectAllStateDirty = 0;
    grSetViewport(&st, 400, 500, 0, 100);
    CHECK(st.renderScissor.x0 == 0 && st.renderScissor.x1 == 0);
    CHECK(st.renderScissor.y0 == 0 && st.renderScissor.y1 == 0);
    /* scissor clipping only applies when SCISSOR_TEST is enabled (faithful) */
    grSetScissor(&st, 5, 200, 30, 300);
    CHECK(st.scissorRectangle.x1 == 200);
    CHECK(st.surfaceScissor.x1 == 320 && (HGuint32)st.surfaceScissor.y1 == 240u); /* ignored */
    grSetCapability(&st, GR_CAPABILITY_SCISSOR_TEST, 1);
    grSetScissor(&st, 5, 200, 30, 300);
    CHECK(st.surfaceScissor.x0 == 5 && st.surfaceScissor.x1 == 200);
    CHECK(st.surfaceScissor.y0 == 30 && st.surfaceScissor.y1 == 240);
    /* render scissor = surface scissor ∩ viewport (400..500 x 0..100 = empty) */
    CHECK(st.renderScissor.x0 == 0 && (HGuint32)st.renderScissor.x1 == 0u);
    grSetViewport(&st, 0, 320, 0, 240);
    grSetScissor(&st, 5, 200, 30, 300);
    CHECK(st.renderScissor.x0 == 5 && st.renderScissor.x1 == 200);
    CHECK(st.renderScissor.y0 == 30 && st.renderScissor.y1 == 240);
    (void)0;

    /* depth range 0.25..0.75 in 1.31 -> fixed(HO,30) */
    grSetDepthRange(&st, FIN(0.25f), FIN(0.75f));
    CHECK((HGuint32)st.zNear == (HGuint32)FIN(0.25f) && (HGuint32)st.zFar == (HGuint32)FIN(0.75f));
    {
        int zn = IScalar_toFixedHO(IScalar_clampUnit(FIN(0.25f)), 30);
        int zf = IScalar_toFixedHO(IScalar_clampUnit(FIN(0.75f)), 30);
        CHECK(st.viewportScale.normal[2] == ((zf - zn) >> 1));
        CHECK(st.viewportOffset.normal[2] == ((zn + zf + 1) >> 1));
    }
    CHECK(st.pixelPipelineDirty == 1);
}

static void test_state_pixel(void)
{
    Gerbera_s st;
    memset(&st, 0, sizeof(st));

    grSetClearColor(&st, 0x8040C0FFu);
    CHECK(st.clearColor == 0x8040C0FFu);
    grSetClearDepth(&st, 0x40000000);       /* 0.5 in 1.31 */
    CHECK(st.clearDepth == 0x7FFFFFFF);     /* (2*d*(2^32-1)) >> 32 */
    grSetClearStencil(&st, 0xAB);
    CHECK(st.clearStencil == 0xAB);

    grSetAlphaFunc(&st, GR_FUNC_GEQUAL, FIN(0.5f));
    CHECK(st.alphaFunc == GR_FUNC_GEQUAL);
    CHECK(st.alphaReference == 128);        /* scalarToFixed(ref, 8) */
    CHECK(st.pixelPipelineDirty == 1 && st.rejectAllStateDirty == 1);
    grSetStencilFunc(&st, GR_FUNC_NOTEQUAL, 7, 0xFF);
    CHECK(st.stencilFunc == GR_FUNC_NOTEQUAL && st.stencilTestMask == 0xFF && st.stencilReference == 7);
    grSetStencilOp(&st, GR_STENCILOP_KEEP, GR_STENCILOP_INCR, GR_STENCILOP_REPLACE);
    CHECK(st.stencilFail == GR_STENCILOP_KEEP);
    CHECK(st.stencilZFail == GR_STENCILOP_INCR && st.stencilZPass == GR_STENCILOP_REPLACE);
    grSetDepthFunc(&st, GR_FUNC_LEQUAL);
    CHECK(st.depthFunc == GR_FUNC_LEQUAL);
    grSetLogicOp(&st, GR_LOGICOP_XOR);
    CHECK(st.logicOp == GR_LOGICOP_XOR);
    grSetBlendFunc(&st, 2, 3);
    CHECK(st.srcBlendFactor == 2 && st.dstBlendFactor == 3);
    grSetColorMask(&st, 0x5);
    CHECK(st.colorMask == 0x5);
    grSetDepthMask(&st, 0);
    CHECK(st.depthMask == 0);
    grSetStencilMask(&st, 0xF0);
    CHECK(st.stencilWriteMask == 0xF0);
    grSetDepthOffset(&st, -2, 4);
    CHECK(st.depthOffsetFactor == -2 && st.depthOffsetUnits == 4);
    CHECK(grIsDepthOffsetEnabled(&st, 0) == grTestCapability(&st, 11));

    /* fog */
    grSetFogMode(&st, GR_FOGMODE_LINEAR);
    CHECK(st.fogMode == GR_FOGMODE_LINEAR);
    grSetFogDensity(&st, FIN(1.5f));
    CHECK((HGuint32)st.fogDensity == (HGuint32)FIN(1.5f));
    grSetFogRange(&st, FIN(10.0f), FIN(20.0f));
    {
        int range = IScalar_sub(FIN(20.0f), FIN(10.0f));
        int oo = IScalar_rcp(range);
        int v = IScalar_scalarToFixed(IScalar_mul(FIN(20.0f), oo), 16);
        CHECK(st.fogOORange == oo);
        CHECK(st.fogAtZero == (HGuint32)~(unsigned int)(unsigned short)hgClamp32_0_x(v, 0xFFFF));
    }
    grSetFogRange(&st, FIN(5.0f), FIN(5.0f));   /* equal -> OOR zero, fog@0 = 0xFFFF */
    CHECK(st.fogOORange == IScalar_zero());
    CHECK(st.fogAtZero == 0xFFFF);
    grSetFogColor(&st, 0x00FFFFFFu);
    CHECK(st.fogColor == 0x00FFFFFFu);
    grSetRectGradients(&st, 1);
    CHECK(st.screenRectGradients == 1);

    /* global ambient */
    {
        _XColor3 amb;
        XColor3_setx(&amb, 100, 200, 300);
        grSetGlobalAmbient(&st, &amb);
        CHECK(st.ambientColor.r == amb.r && st.ambientColor.g == amb.g && st.ambientColor.b == amb.b);
        CHECK(st.ambientLightSumDirty == 1);
        st.ambientLightSumDirty = 0;
        grSetGlobalAmbient(&st, &amb);      /* same: no dirty */
        CHECK(st.ambientLightSumDirty == 0);
    }

    /* draw surface descriptor roundtrip */
    {
        _grSurfaceDescriptor desc;
        _grDrawSurface *ds = &st.drawSurface;
        ds->rgbaOffsets[0] = 0; ds->rgbaOffsets[1] = 8;
        ds->rgbaOffsets[2] = 16; ds->rgbaOffsets[3] = 24;
        ds->rgbaSizes[0] = 8; ds->rgbaSizes[1] = 8;
        ds->rgbaSizes[2] = 8; ds->rgbaSizes[3] = 8;
        ds->colorMask = 0xF;
        ds->pixelFormat = GR_IMAGEFORMAT_RGBA;
        grGetDrawBufferDescriptor(&st, &desc);
        CHECK(desc.rgbaOffsets[0] == 0 && desc.rgbaOffsets[3] == 24);
        CHECK(desc.rgbaSizes[0] == 8 && desc.rgbaSizes[3] == 8);
        CHECK(desc.colorMask == 0xF && desc.pixelFormat == GR_IMAGEFORMAT_RGBA);
        (void)ds;
    }

    /* dither decision: any channel masked in with size 1..7 */
    grSetCapability(&st, GR_CAPABILITY_DITHER, 1);
    st.drawSurface.colorMask = 0xF;
    st.drawSurface.rgbaSizes[0] = 8; st.drawSurface.rgbaSizes[1] = 8;
    st.drawSurface.rgbaSizes[2] = 8; st.drawSurface.rgbaSizes[3] = 8;
    grUpdateDither(&st);
    CHECK(st.useDithering == 0);            /* all channels 8-bit: no dither */
    st.drawSurface.rgbaSizes[1] = 5;
    grUpdateDither(&st);
    CHECK(st.useDithering == 1);            /* G 5-bit: dither */
    st.drawSurface.colorMask = 0xC;         /* bits 2,3: G (bit 1) not written */
    grUpdateDither(&st);
    CHECK(st.useDithering == 0);
}

static void test_state_material(void)
{
    Gerbera_s st;
    _XColor3 c1, c2;
    memset(&st, 0, sizeof(st));

    XColor3_setx(&c1, 100, 200, 300);
    grSetMaterialColor(&st, 0, &c1);
    CHECK(st.material.colors[0].r == c1.r && st.material.colors[0].b == c1.b);
    CHECK(st.baseColorDirty == 1);          /* AMBIENT slot */
    st.baseColorDirty = 0;
    grSetMaterialColor(&st, 0, &c1);        /* same: no dirty */
    CHECK(st.baseColorDirty == 0);
    XColor3_setx(&c2, 9, 9, 9);
    grSetMaterialColor(&st, 3, &c2);        /* EMISSION slot */
    CHECK(st.baseColorDirty == 1);
    st.baseColorDirty = 0;
    grSetMaterialColor(&st, 1, &c2);
    CHECK(st.lightColorDirtyMask == 0xFFu && st.baseColorDirty == 0);

    grSetMaterialAlpha(&st, 0xBEEFu);
    CHECK(st.material.alpha == 0xBEEFu);
    grSetMaterialShininess(&st, FIN(0.25f));
    CHECK((HGuint32)grGetMaterialShininess(&st) == (HGuint32)FIN(0.25f));
}
