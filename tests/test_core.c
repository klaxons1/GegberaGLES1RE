/* test_core.c - checks for the gerbera core foundational layer (grDefs.h). */
#include "gerbera/grDefs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
static void test_state_lights(void);
static void test_transform(void);
static void test_update_lights(void);
static void test_itc_transform(void);
static void test_fetch_stages(void);
static void test_update_pipeline(void);
static void test_transform_vertex(void);

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
    test_state_lights();
    test_transform();
    test_update_lights();
    test_itc_transform();
    test_fetch_stages();
    test_update_pipeline();
    test_transform_vertex();
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

/* --- transform stack + user clip planes + ITC transform -------------------- */

static void test_transform(void)
{
    Gerbera_s st;
    _IMatrix4x4 id;
    _IFVector4 eq;
    _ITCTransform itc;
    int ci;

    memset(&st, 0, sizeof(st));
    IMatrix4x4_identity(&id);

    /* grSetMatrix: copy + dirty bit; MODELVIEW=0 / PROJECTION=1 (EPOC slots) */
    grSetMatrix(&st, GR_MATRIXMODE_MODELVIEW, &id);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_MODELVIEW) == 1);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_PROJECTION) == 0);
    for (ci = 0; ci < 4; ++ci)
        CHECK(st.matrix[0].v[ci].e == id.v[ci].e &&
              st.matrix[0].v[ci].m[0] == id.v[ci].m[0] &&
              st.matrix[0].v[ci].m[3] == id.v[ci].m[3]);
    grSetMatrix(&st, GR_MATRIXMODE_PROJECTION, &id);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_PROJECTION) == 1);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_TEXTURE0) == 0);

    /* grSetMatrixDirty: set/clear one bit at a time */
    grSetMatrixDirty(&st, GR_MATRIXMODE_TEXTURE1, 1);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_TEXTURE1) == 1);
    grSetMatrixDirty(&st, GR_MATRIXMODE_TEXTURE1, 0);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_TEXTURE1) == 0);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_MODELVIEW) == 1);  /* untouched */

    /* user clip planes: planes 10..15 -> slots 0..5 (EPOC bases verified) */
    IFVector4_set(&eq, FIN(0.0f), FIN(1.0f), FIN(0.0f), FIN(-0.5f));
    grSetUserClipPlane(&st, GR_CLIP_USER0 + 2, &eq, &id);
    CHECK(st.origUserClipPlanes[2].m[0] == eq.m[0] &&
          st.origUserClipPlanes[2].m[3] == eq.m[3]);
    CHECK(st.origUserClipMatrices[2].v[0].e == id.v[0].e);
    CHECK(grGetUserClipPlane(&st, GR_CLIP_USER0 + 2) == &st.origUserClipPlanes[2]);
    grSetUserClipPlane(&st, GR_CLIP_USER0 + 5, &eq, &id);
    CHECK(grGetUserClipPlane(&st, GR_CLIP_USER0 + 5) == &st.origUserClipPlanes[5]);

    /* ITCTransform_copy: identity -> lsh=8 (127-119), rsh=0, im row 3 =
     * 10.21 ones, classification 2-bits on the diagonal */
    ITCTransform_copy(&itc, &id);
    for (ci = 0; ci < 4; ++ci)
    {
        CHECK(itc.e[ci] == (0x7F << (8 * ci)));
        CHECK(itc.lsh[ci][ci] == 8 && itc.rsh[ci][ci] == 0);
        CHECK(itc.im[ci] == (ci == 3 ? 2097152 : 0));
    }
    CHECK(itc.cl == (int)0x80200802u);
}

/* --- grUpdateLight / grUpdateLights ----------------------------------------- */

static void test_update_lights(void)
{
    Gerbera_s st;
    _IFVector4 pos;
    _XColor3 ac, lc, ma, me;
    int ret;

    /* grUpdateLight: directional -> mask 0x40 + headlight halfway =
     * normalize(nVP + (0,0,2^21)); probed value pinned below */
    memset(&st, 0, sizeof(st));
    IFVector4_set(&pos, FIN(2.0f), FIN(0.0f), FIN(0.0f), FIN(0.0f));
    grSetLightPosition(&st, 0, &pos);
    grSetLightSpotCutoff(&st, 0, FIN(180.0f));
    CHECK(grUpdateLight(&st, 0) == 0x40u);
    CHECK(st.lights[0].lightMask == 0x40);
    CHECK(st.lights[0].halfway[0] == 1482910);
    CHECK(st.lights[0].halfway[1] == 0);
    CHECK(st.lights[0].halfway[2] == 1482910);

    /* positional spotlight: bit 16 (cutoff != 180) + bit 32 (attn != 1,1,1),
     * halfway untouched */
    memset(&st, 0, sizeof(st));
    IFVector4_set(&pos, FIN(1.0f), FIN(2.0f), FIN(3.0f), FIN(1.0f));
    grSetLightPosition(&st, 2, &pos);
    grSetLightSpotCutoff(&st, 2, FIN(60.0f));
    CHECK(grUpdateLight(&st, 2) == 0x30u);
    CHECK(st.lights[2].halfway[0] == 0 && st.lights[2].halfway[2] == 0);

    /* grUpdateLights: one directional + one positional, full refresh */
    memset(&st, 0, sizeof(st));
    XColor3_setx(&ac, 65536, 65536, 65536);       /* 2^21 = 1.0 (10.21) */
    XColor3_setx(&lc, 32768, 16384, 8192);
    XColor3_setx(&ma, 65536, 65536, 65536);
    XColor3_setx(&me, 16384, 16384, 16384);
    grSetGlobalAmbient(&st, &ac);
    IFVector4_set(&pos, FIN(2.0f), FIN(0.0f), FIN(0.0f), FIN(0.0f));
    grSetLightPosition(&st, 0, &pos);
    grSetLightSpotCutoff(&st, 0, FIN(180.0f));
    grSetLightColor(&st, 0, 0, &lc);
    IFVector4_set(&pos, FIN(1.0f), FIN(2.0f), FIN(3.0f), FIN(1.0f));
    grSetLightPosition(&st, 1, &pos);
    grSetLightSpotCutoff(&st, 1, FIN(180.0f));
    grSetMaterialColor(&st, 0, &ma);
    grSetMaterialColor(&st, 3, &me);
    grSetCapability(&st, GR_CAPABILITY_LIGHT0, 1);
    grSetCapability(&st, GR_CAPABILITY_LIGHT0 + 1, 1);
    ret = grUpdateLights(&st);
    CHECK(ret == 1);                                  /* positional light on */
    CHECK(st.lightDirectionalMask == 1);
    CHECK(st.lightActiveDirtyMask == 0);
    CHECK(st.lightTypeDirtyMask == 0);
    CHECK((st.lightColorDirtyMask & 3u) == 0);        /* cleared for enabled */
    CHECK(st.lights[0].lightMask == 0x40);            /* directional, no premul */
    CHECK(st.lights[1].lightMask == 0x20);            /* positional, zero colors */
    CHECK(st.l_directional[0] == &st.lights[0]);
    CHECK(st.l_directional[1] == NULL);
    CHECK(st.l_positional[0] == &st.lights[1]);
    CHECK(st.l_positional[1] == NULL);
    /* ambientLightSum = ambient + light0 ambient */
    CHECK(st.ambientLightSum.r == 3145728);
    CHECK(st.ambientLightSum.g == 2621440);
    CHECK(st.ambientLightSum.b == 2359296);
    /* baseColor = sum * material.ambient + material.emission */
    CHECK(st.baseColor.r == 3670016);
    CHECK(st.baseColor.g == 3145728);
    CHECK(st.baseColor.b == 2883584);
    CHECK(st.ambientLightSumMask == 1);
    CHECK(st.ambientLightSumDirty == 0);
    CHECK(st.baseColorDirty == 0);

    /* second run: idempotent (all asserts inside grUpdateLights hold) */
    ret = grUpdateLights(&st);
    CHECK(ret == 1);
    CHECK(st.ambientLightSum.r == 3145728);
    CHECK(st.baseColor.r == 3670016);

    /* disable the positional light -> partition drops it, return 0 */
    grSetCapability(&st, GR_CAPABILITY_LIGHT0 + 1, 0);
    ret = grUpdateLights(&st);
    CHECK(ret == 0);
    CHECK(st.l_positional[0] == NULL);
    CHECK(st.l_directional[0] == &st.lights[0]);
}

/* --- ITCTransform use (mul family + DivByQ) --------------------------------- */

static void test_itc_transform(void)
{
    _IMatrix4x4 id;
    _ITCTransform itc;
    int out[2];
    int st2[4];
    int fl[4];

    IMatrix4x4_identity(&id);
    ITCTransform_copy(&itc, &id);

    /* mulxv2: 16.16 in -> 10.21 out (identity) */
    st2[0] = 65536; st2[1] = 32768;
    ITCTransform_mulxv2(out, &itc, st2);
    CHECK(out[0] == 2097152 && out[1] == 1048576);
    /* mulxv4 */
    st2[0] = 65536; st2[1] = 65536; st2[2] = 0; st2[3] = 65536;
    ITCTransform_mulxv4(out, &itc, st2);
    CHECK(out[0] == 2097152 && out[1] == 2097152);
    /* mulxv n=3 pads q=1 (16.16) */
    st2[0] = 65536; st2[1] = 32768; st2[2] = 12345;
    ITCTransform_mulxv(out, &itc, st2, 3);
    CHECK(out[0] == 2097152 && out[1] == 1048576);
    /* mulfv2 with IScalar ones */
    st2[0] = IScalar_one(); st2[1] = IScalar_one();
    ITCTransform_mulfv2(out, &itc, st2);
    CHECK(out[0] == 2097152 && out[1] == 2097152);
    /* mulfv with float components {1, 0.5, 0, 1} */
    fl[0] = FIN(1.0f); fl[1] = FIN(0.5f); fl[2] = FIN(0.0f); fl[3] = FIN(1.0f);
    ITCTransform_mulfv(out, &itc, fl, 4);
    CHECK(out[0] == 2097152 && out[1] == 1048576);

    /* grTexCoordDivByQ: q = 1.0 (2^21) / 0 pass through */
    grTexCoordDivByQ(out, (int[]){100, 200, 0, 0x200000});
    CHECK(out[0] == 100 && out[1] == 200);
    grTexCoordDivByQ(out, (int[]){100, 200, 0, 0});
    CHECK(out[0] == 100 && out[1] == 200);
    /* q = 0.5: x/0.5 = 2x. The rcp approximation loses one LSB - probed
     * value from the faithful implementation. */
    grTexCoordDivByQ(out, (int[]){100, 200, 0, 1048576});
    CHECK(out[0] == 199 && out[1] == 399);
}

/* --- fetch / transform / fog stages (needs a fake render stack) -------------- */

static void test_fetch_stages(void)
{
    Gerbera_s st;
    _IMatrix4x4 id;
    unsigned char *pool = (unsigned char *)malloc(1332 + 2048);
    unsigned char colors[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    unsigned char tc[4] = {10, 20, 30, 40};
    unsigned char list[3] = {0, 1, 0xFF};
    _grVertex *vb;

    memset(&st, 0, sizeof(st));
    IMatrix4x4_identity(&id);
    st.renderStack = pool;
    vb = (_grVertex *)(pool + 1332);
    vb[0].index = 0;
    vb[1].index = 1;

    /* color fast path: u8 -> u16 rows (lit + secondary), alpha only row 0 */
    st.vertexArray[2].data = colors;
    st.vertexArray[2].stride = 4;
    st.vertexArray[2].type = GR_DATATYPE_UINT8;
    grFetchVertexColorFastUINT8(&st, list);
    CHECK(vb[0].color.final[0][0] == 0x1000);
    CHECK(vb[0].color.final[0][1] == 0x2000);
    CHECK(vb[0].color.final[0][2] == 0x3000);
    CHECK(vb[0].color.final[0][3] == 0x4000);
    CHECK(vb[0].color.final[1][0] == 0x1000);
    CHECK(vb[0].color.final[1][1] == 0x2000);
    CHECK(vb[0].color.final[1][2] == 0x3000);
    CHECK(vb[1].color.final[0][0] == 0x5000);

    /* color lighting path: XColor3 ints + doubled alpha */
    memset(pool + 1332, 0, 1024);
    vb[0].index = 0; vb[1].index = 1;
    grFetchVertexColorUINT8(&st, list);
    CHECK(vb[0].color.input.rgb[0].r == 131584);   /* (b<<13) | 32*b */
    CHECK(vb[0].color.input.rgb[0].g == 263168);
    CHECK(vb[0].color.input.rgb[0].b == 394752);
    CHECK(vb[0].color.final[1][2] == 0x4040);

    /* texcoords INT8x2 through the identity transform: 10.21 out */
    st.vertexArray[6].data = tc;
    st.vertexArray[6].stride = 2;
    st.vertexArray[6].type = GR_DATATYPE_INT8;
    st.vertexArray[6].size = 2;
    ITCTransform_copy(&st.texCoordTransform[0], &id);
    grTransformTexCoordINT8x2(&st, list, 0);
    CHECK(vb[0].texCoord[0].x == 10 * 2097152);
    CHECK(vb[0].texCoord[0].y == 20 * 2097152);
    CHECK(vb[1].texCoord[0].x == 30 * 2097152);
    CHECK(vb[1].texCoord[0].y == 40 * 2097152);

    /* point size FIXED: clamp [1.0, 64.0] in 16.16 */
    {
        unsigned int sizes[2] = {0x20000, 0x800000};
        st.vertexArray[5].data = sizes;
        st.vertexArray[5].stride = 4;
        grFetchPointSizeFIXED(&st, list);
        CHECK(vb[0].pointSize == 0x20000);
        CHECK(vb[1].pointSize == 0x400000);
    }

    /* fog FAST: full fog row */
    memset(pool + 1332, 0, 1024);
    vb[0].index = 0; vb[1].index = 1;
    grApplyFogFAST(&st, list);
    CHECK(vb[0].color.final[1][3] == 0xFFFF);
    CHECK(vb[1].color.final[1][3] == 0xFFFF);

    free(pool);
}

/* --- grUpdateVertexPipeline (stage selection) -------------------------------- */

static void test_update_pipeline(void)
{
    Gerbera_s st;
    _IMatrix4x4 id;

    memset(&st, 0, sizeof(st));
    IMatrix4x4_identity(&id);
    /* grInitState defaults: attenuation (1,0,0) */
    st.pointDistAttn.e = 127;
    st.pointDistAttn.m[0] = 0x20000000;

    /* lighting off: fast-default color, no normals */
    grUpdateVertexPipeline(&st);
    CHECK(st.fetchVertexColor == grFetchVertexColorFastDEFAULT);
    CHECK(st.fetchVertexNormal == NULL);
    CHECK(st.fetchPointSize == NULL);
    CHECK(st.applyFog == NULL);                    /* FOG cap off */
    CHECK(st.transformVE == 0);
    CHECK(st.vertexPipelineDirty == 0);
    /* changing pointDistAttn flags the pipeline dirty */
    st.pointDistAttn.m[0] = 0x10000000;
    grUpdateVertexPipeline(&st);
    CHECK(st.computePointDistAttn == 1);
    CHECK(st.transformVE == 1);
    CHECK(st.vertexPipelineDirty == 1);
    st.pointDistAttn.m[0] = 0x20000000;
    grUpdateVertexPipeline(&st);
    CHECK(st.vertexPipelineDirty == 1);
    grUpdateVertexPipeline(&st);
    CHECK(st.vertexPipelineDirty == 0);

    /* fog on, LINEAR mode (0) + fogOORange != 0 -> LINEAR; == 0 -> FAST */
    grSetCapability(&st, GR_CAPABILITY_FOG, 1);
    st.colorMask = 0xF;
    st.fogOORange = 0x12345;
    grUpdateVertexPipeline(&st);
    CHECK(st.applyFog == grApplyFogLINEAR);
    st.fogOORange = 0;
    grUpdateVertexPipeline(&st);
    CHECK(st.applyFog == grApplyFogFAST);
    /* fog mode EXP */
    grSetFogMode(&st, GR_FOGMODE_EXP);
    grUpdateVertexPipeline(&st);
    CHECK(st.applyFog == grApplyFogEXP);
    /* fog mode EXP2 */
    grSetFogMode(&st, GR_FOGMODE_EXP2);
    grUpdateVertexPipeline(&st);
    CHECK(st.applyFog == grApplyFogEXP2);

    /* point size array: POINT primitive + cap + data -> FLOAT/FIXED */
    {
        unsigned int sizes[2] = {0x20000, 0x20000};
        st.primitiveType = GR_PRIMITIVETYPE_POINT;
        st.vertexArray[5].data = sizes;
        st.vertexArray[5].stride = 4;
        st.vertexArray[5].type = GR_DATATYPE_FLOAT;
        grSetCapability(&st, GR_CAPABILITY_POINT_SIZE_ARRAY, 1);
        grUpdateVertexPipeline(&st);
        CHECK(st.fetchPointSize == grFetchPointSizeFLOAT);
        st.vertexArray[5].type = GR_DATATYPE_FIXED;
        grUpdateVertexPipeline(&st);
        CHECK(st.fetchPointSize == grFetchPointSizeFIXED);
    }

    /* texture matrix dirty -> texCoordTransform refreshed; unit enabled with
     * no array -> default texcoords transformed (1,0) -> (2^21, 0) */
    grSetMatrix(&st, GR_MATRIXMODE_TEXTURE0, &id);
    st.textureUnitEnabled[0] = 1;
    st.defaultTexCoord.f[0][0] = 1.0f;
    st.defaultTexCoord.f[0][3] = 1.0f;
    grUpdateVertexPipeline(&st);
    CHECK(grIsMatrixDirty(&st, GR_MATRIXMODE_TEXTURE0) == 0);
    CHECK(st.texCoordTransform[0].cl == (int)0x80200802u);
    CHECK(st.fetchTexCoord[0] == grTransformTexCoordDEFAULT);
    CHECK(st.defaultTexCoord.x[2][0] == 2097152);
    CHECK(st.defaultTexCoord.x[2][1] == 0);
    /* disabled unit -> NULL */
    CHECK(st.fetchTexCoord[1] == NULL);
}

/* --- grTransformVertex / grFetchWeightAndIndex / clip codes ------------------ */

static void test_transform_vertex(void)
{
    Gerbera_s st;
    _IMatrix4x4 id;
    _IFVector4 eq;
    unsigned char *pool = (unsigned char *)malloc(1332 + 2048);
    _grVertex *vb;
    float verts[8] = {1.0f, 2.0f, 3.0f, 1.0f, -1.0f, 0.5f, 0.25f, 1.0f};
    unsigned char k;

    memset(&st, 0, sizeof(st));
    IMatrix4x4_identity(&id);
    IVertexTransform_copy(&st.objectToClipTransform, &id);
    st.renderStack = pool;
    vb = (_grVertex *)(pool + 1332);
    st.vertexArray[0].data = verts;
    st.vertexArray[0].stride = 16;
    st.vertexArray[0].size = 4;
    st.vertexArray[0].type = GR_DATATYPE_FLOAT;

    /* float (1,2,3,1) through the identity: clip = same, e = max exp byte,
     * m shifted down per component; clip codes: z>w -> bit 1, y>w -> bit 9 */
    vb[0].index = 0;
    grTransformVertex(&st, &vb[0]);
    CHECK(grGetVertexPositionState(&vb[0]) == GR_POSITION_OBJECT_SPACE_FLOAT);
    CHECK((vb[0].stateMask & GR_VERTEXMASK_TRANSFORMED) != 0);
    CHECK(vb[0].clipPosition.e == 0x80);
    CHECK(vb[0].clipPosition.m[0] == 268435456);       /* 1.0 */
    CHECK(vb[0].clipPosition.m[1] == 536870912);       /* 2.0 */
    CHECK(vb[0].clipPosition.m[2] == 805306368);       /* 3.0 */
    CHECK(vb[0].clipPosition.m[3] == 268435456);       /* 1.0 */
    CHECK(vb[0].clipMask == 0x202u);
    /* (-1, 0.5, 0.25, 1): all inside */
    vb[1].index = 1;
    grTransformVertex(&st, &vb[1]);
    CHECK(vb[1].clipMask == 0);
    CHECK(vb[1].clipPosition.m[0] == -536870912);

    /* fixed 16.16 (1, -2, 0.5, 1): int view {x,y,z,w} through the identity */
    memset(pool + 1332, 0, 512);
    memset(&st, 0, sizeof(st));
    IVertexTransform_copy(&st.objectToClipTransform, &id);
    st.renderStack = pool;
    vb = (_grVertex *)(pool + 1332);
    {
        int fverts[4] = {65536, -131072, 32768, 65536};
        st.vertexArray[0].data = fverts;
        st.vertexArray[0].stride = 16;
        st.vertexArray[0].size = 4;
        st.vertexArray[0].type = GR_DATATYPE_FIXED;
        vb[0].index = 0;
        grTransformVertex(&st, &vb[0]);
        CHECK(grGetVertexPositionState(&vb[0]) == GR_POSITION_OBJECT_SPACE_FIXED);
        CHECK(vb[0].clipPosition.m[0] == 268435456);
        CHECK(vb[0].clipPosition.m[1] == -536870912);
        CHECK(vb[0].clipPosition.m[2] == 134217728);
        CHECK(vb[0].clipPosition.m[3] == 268435456);
    }

    /* user clip plane x <= 2 on slot 5, vertex (1,0,0,1): inside; plane
     * x <= 0.5: outside -> clip bit = digit+9 = 15 */
    memset(pool + 1332, 0, 512);
    memset(&st, 0, sizeof(st));
    IVertexTransform_copy(&st.objectToClipTransform, &id);
    st.renderStack = pool;
    vb = (_grVertex *)(pool + 1332);
    IFVector4_set(&eq, FIN(1.0f), FIN(0.0f), FIN(0.0f), FIN(-2.0f));
    grSetUserClipPlane(&st, GR_CLIP_USER0 + 5, &eq, &id);
    grSetCapability(&st, 30 + 5, 1);
    st.enabledUserClipPlaneIndices = 6;                /* plane 5 -> digit 6 */
    {
        float v2[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        st.vertexArray[0].data = v2;
        st.vertexArray[0].stride = 16;
        st.vertexArray[0].size = 4;
        st.vertexArray[0].type = GR_DATATYPE_FLOAT;
        vb[0].index = 0;
        /* the clip-plane transform is grSetupCulling's job; do it by hand:
         * identity modelview keeps the equation */
        st.userClipPlanes[5] = st.origUserClipPlanes[5];
        grTransformVertex(&st, &vb[0]);
        CHECK(st.userClipPlanes[5].m[0] == 536870912); /* 6.26 mantissa of 1.0 */
        CHECK(st.userClipPlanes[5].m[3] == -536870912);/* of -2.0 */
        CHECK(vb[0].clipMask == 0x8000u);              /* bit 6+9: 1-2 < 0 */
        /* grIsOutsideUserClipPlane directly */
        CHECK(grIsOutsideUserClipPlane(&st.userClipPlanes[5], &vb[0].clipPosition) == 1);
    }

    /* grFetchWeightAndIndex: FIXED weights, UINT8 indices (9 clamps to 8) */
    {
        int wfix[2] = {65536, 65536};
        unsigned char idx[2] = {3, 9};
        int weights[4];
        unsigned int packed;
        st.vertexArray[3].data = wfix;
        st.vertexArray[3].stride = 8;
        st.vertexArray[3].size = 2;
        st.vertexArray[3].type = GR_DATATYPE_FIXED;
        st.vertexArray[4].data = idx;
        st.vertexArray[4].stride = 2;
        st.vertexArray[4].size = 2;
        st.vertexArray[4].type = GR_DATATYPE_UINT8;
        packed = grFetchWeightAndIndex(&st, weights, 0);
        CHECK(weights[0] == 1065353216);               /* IScalar 1.0 */
        CHECK(weights[1] == 1065353216);
        CHECK(packed == 0x0803u);                      /* bytes: 3, 8 */
    }

    /* matrix palette path: identity matrices, weight (1,0), indices (0,1):
     * position (1,2,3,1) unchanged, state -> EYE_SPACE, clip via projection */
    memset(pool + 1332, 0, 512);
    memset(&st, 0, sizeof(st));
    IVertexTransform_copy(&st.objectToClipTransform, &id);
    IVertexTransform_copy(&st.projectionTransform, &id);
    for (k = 0; k < 9; ++k)
        IVertexTransform_copy(&st.matrixPalTransform[k], &id);
    st.renderStack = pool;
    vb = (_grVertex *)(pool + 1332);
    {
        float v3[4] = {1.0f, 2.0f, 3.0f, 1.0f};
        int wfix[2] = {65536, 0};
        unsigned char idx[2] = {0, 1};
        st.vertexArray[0].data = v3;
        st.vertexArray[0].stride = 16;
        st.vertexArray[0].size = 4;
        st.vertexArray[0].type = GR_DATATYPE_FLOAT;
        st.vertexArray[3].data = wfix;
        st.vertexArray[3].stride = 8;
        st.vertexArray[3].size = 2;
        st.vertexArray[3].type = GR_DATATYPE_FIXED;
        st.vertexArray[4].data = idx;
        st.vertexArray[4].stride = 2;
        st.vertexArray[4].size = 2;
        st.vertexArray[4].type = GR_DATATYPE_UINT8;
        grSetCapability(&st, GR_CAPABILITY_MATRIX_PALETTE, 1);
        grSetCapability(&st, GR_CAPABILITY_MATRIX_INDEX_ARRAY, 1);
        grSetCapability(&st, GR_CAPABILITY_WEIGHT_ARRAY, 1);
        vb[0].index = 0;
        grTransformVertex(&st, &vb[0]);
        CHECK(grGetVertexPositionState(&vb[0]) == GR_POSITION_EYE_SPACE);
        /* accfv output: per-component exponent bytes, m normalized */
        CHECK(vb[0].position.objectSpaceFloat[0].e == 0x7f80807fu);
        CHECK(vb[0].position.objectSpaceFloat[0].m[0] == 536870912);
        CHECK(vb[0].position.objectSpaceFloat[0].m[1] == 536870912);
        CHECK(vb[0].position.objectSpaceFloat[0].m[2] == 805306368);
        CHECK(vb[0].position.objectSpaceFloat[0].m[3] == 536870912);
        CHECK(vb[0].clipPosition.e == 0x80);
        CHECK(vb[0].clipPosition.m[0] == 268435456);
        CHECK(vb[0].clipPosition.m[2] == 805306368);
        CHECK(vb[0].clipMask == 0x202u);
    }

    /* vertex record size: the render stack addresses vertices with << 7 */
    CHECK(sizeof(_grVertex) == 128);
    free(pool);
}

static void test_state_lights(void)
{
    Gerbera_s st;
    _IFVector4 pos;
    _IFVector3 orgDir;
    _XColor3 c;
    int dir[3];
    int i;

    memset(&st, 0, sizeof(st));
    IFVector3_setf(&orgDir, 0, FIN(0.0f));
    IFVector3_setf(&orgDir, 1, FIN(-1.0f));
    IFVector3_setf(&orgDir, 2, FIN(0.0f));

    /* positional light at (1,2,3,1) */
    IFVector4_set(&pos, FIN(1.0f), FIN(2.0f), FIN(3.0f), FIN(1.0f));
    grSetLightPosition(&st, 0, &pos);
    CHECK(st.lights[0].positional == 1);
    CHECK(fabs(IScalar_stof(IFVector4_getf(&st.lights[0].orgPosition, 1)) - 2.0f) < 1e-6f);
    CHECK((st.lightTypeDirtyMask & 1u) != 0);

    /* directional light at (2,0,0,0): w==0 -> positional==0, nVP normalized */
    IFVector4_set(&pos, FIN(2.0f), FIN(0.0f), FIN(0.0f), FIN(0.0f));
    grSetLightPosition(&st, 1, &pos);
    CHECK(st.lights[1].positional == 0);
    CHECK(abs(st.lights[1].nVP[0] - 2097152) <= 4);   /* (1,0,0) in 10.21 */
    CHECK(st.lights[1].nVP[1] == 0);

    /* spot direction: stored negated */
    dir[0] = 0; dir[1] = 2097152; dir[2] = 0;
    grSetLightSpotDirection(&st, 2, dir, &orgDir);
    CHECK(st.lights[2].spotDirectionNegated[1] == -2097152);
    CHECK(fabs(IScalar_stof(IFVector3_getf(&st.lights[2].orgSpotDirection, 1)) + 1.0f) < 1e-6f);

    /* colors: ambient(0)/diffuse(1)/specular(2).
     * NOTE setx scales x32; XScalar_mul_sat(a,b) = (a*b)>>21, so small
     * colors multiply to 0 - use large colors for the premul check. */
    XColor3_setx(&c, 10, 20, 30);
    for (i = 0; i < 3; ++i)
        grSetLightColor(&st, 0, i, &c);
    CHECK((st.lightColorDirtyMask & 1u) != 0);
    for (i = 0; i < 3; ++i)
    {
        _XColor3 out;
        grGetLightColor(&st, 0, i, &out);
        CHECK(out.r == c.r && out.g == c.g && out.b == c.b);
    }
    /* nonzero colors -> grTestLightColor; premul needs big colors */
    CHECK(grTestLightColor(&st.lights[0], 0) != 0);
    CHECK(grTestLightColor(&st.lights[3], 0) == 0);   /* untouched light: zero */
    {
        _XColor3 big;
        XColor3_setx(&big, 65536, 65536, 65536);      /* 2^21 = 1.0 (10.21) */
        grSetLightColor(&st, 0, 2, &big);
        CHECK(grPrecalcLightColor(&st.lights[0], 2, &big) != 0);
        CHECK(st.lights[0].premul_colors[2].r == 2097152);
        grSetLightColor(&st, 0, 2, &c);
        CHECK(grPrecalcLightColor(&st.lights[0], 2, &c) == 0);    /* c*c >> 21 = 0 */
    }

    /* attenuation (constant/linear/quadratic) */
    grSetLightAttenuation(&st, 0, 0, FIN(1.0f));
    grSetLightAttenuation(&st, 0, 1, FIN(0.5f));
    grSetLightAttenuation(&st, 0, 2, FIN(0.25f));
    CHECK(fabs(IScalar_stof(grGetLightAttenuation(&st, 0, 1)) - 0.5f) < 1e-6f);
    CHECK(fabs(IScalar_stof(grGetLightAttenuation(&st, 0, 2)) - 0.25f) < 1e-6f);

    /* exponent (XScalar roundtrip) */
    grSetLightExponent(&st, 0, FIN(2.0f));
    CHECK((HGuint32)grGetLightExponent(&st, 0) == (HGuint32)FIN(2.0f));

    /* spot cutoff: stored raw (IScalar degrees) + cosine clamped to [0,1]
     * as XScalar */
    grSetLightSpotCutoff(&st, 0, FIN(60.0f));
    CHECK((HGuint32)grGetLightSpotCutoff(&st, 0) == (HGuint32)FIN(60.0f));
    {
        int cos60 = IScalar_cos(IScalar_degreesToRadians(FIN(60.0f)));
        CHECK(st.lights[0].cosSpotCutoff == IScalar_toXScalar(IScalar_max(cos60, IScalar_zero())));
        /* cos(60) = 0.5: XScalar roundtrip */
        CHECK(fabs((float)st.lights[0].cosSpotCutoff / 2097152.0f - 0.5f) < 1e-3f);
    }
    /* cutoff 180 -> cos = -1 clamped to 0 */
    grSetLightSpotCutoff(&st, 0, FIN(180.0f));
    CHECK(st.lights[0].cosSpotCutoff == 0);
}
