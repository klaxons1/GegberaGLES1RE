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

int main(void)
{
    test_descriptors();
    test_rectangles();
    test_formats();
    test_stack();
    test_rgba();
    test_pack_color();
    test_texture();
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
