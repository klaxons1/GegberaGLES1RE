/* grUtilSmall.c - descriptor<->color utilities (port of the decompiled
 * grUtilSmall.inl; the texture validity/update functions need the texture
 * struct and land with the texture subsystem). */
#include "gerbera/grDefs.h"
#include "gerbera/grRGBA.h"
#include "hg/hgInt32.h"

#define GR_UTIL_SMALL_CHECK(cond, file, line)    \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, (file), (line)) != 0) \
            grDie(0);                            \
    } while (0)

/* GR_COLORMASK_FULL: bits 0..3, one per channel (the mask check rejects
 * everything above 0xF). */
#define GR_COLORMASK_FULL 0xFu

int grIsValidDescriptor(_grSurfaceDescriptor *desc)
{
    GR_UTIL_SMALL_CHECK(desc != NULL, "grUtilSmall.inl", 35);
    GR_UTIL_SMALL_CHECK((desc->colorMask & 0xF0) == 0, "grUtilSmall.inl", 37);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->colorBytes, 0, 4), "grUtilSmall.inl", 38);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->depthBytes, 0, 4), "grUtilSmall.inl", 39);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->stencilBytes, 0, 4), "grUtilSmall.inl", 40);
    GR_UTIL_SMALL_CHECK(desc->padding[0] == 0 && desc->padding[1] == 0, "grUtilSmall.inl", 41);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->pixelFormat, 0, GR_PIXELFORMAT_MAX), "grUtilSmall.inl", 43);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->paletteFormat, 0, GR_PALETTEFORMAT_MAX - 1), "grUtilSmall.inl", 44);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaOffsets[0], 0, 31), "grUtilSmall.inl", 46);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaOffsets[1], 0, 31), "grUtilSmall.inl", 47);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaOffsets[2], 0, 31), "grUtilSmall.inl", 48);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaOffsets[3], 0, 31), "grUtilSmall.inl", 49);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaSizes[0], 0, 8), "grUtilSmall.inl", 51);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaSizes[1], 0, 8), "grUtilSmall.inl", 52);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaSizes[2], 0, 8), "grUtilSmall.inl", 53);
    GR_UTIL_SMALL_CHECK(hgInRange32(desc->rgbaSizes[3], 0, 8), "grUtilSmall.inl", 54);
    return 1;
}

unsigned int grPackColor(unsigned int color, _grSurfaceDescriptor *desc)
{
    unsigned int out = 0;
    int i;

    GR_UTIL_SMALL_CHECK(grIsValidDescriptor(desc) != 0, "grUtilSmall.inl", 239);
    for (i = GR_COLOR_R; i < GR_COLOR_MAX; ++i)
    {
        int offset = grGetDescriptorOffset(desc, i);
        int size = grGetDescriptorSize(desc, i);
        GR_UTIL_SMALL_CHECK(hgInRange32(offset, 0, 31), "grUtilSmall.inl", 245);
        GR_UTIL_SMALL_CHECK(hgInRange32(size, 0, 8), "grUtilSmall.inl", 246);
        out |= (unsigned int)(grUnpackRGBA(color, i) >> (8 - size)) << offset;
    }
    return out;
}

int grMaskedDescriptorsEqual(unsigned int mask, _grSurfaceDescriptor *a, _grSurfaceDescriptor *b)
{
    int i;
    int match = 1;

    GR_UTIL_SMALL_CHECK((mask & 0xFFFFFFF0u) == 0, "grUtilSmall.inl", 296);
    for (i = 0; i < 4; ++i)
    {
        if (((1 << i) & (int)mask) != 0)
            match = (a->rgbaSizes[i] == b->rgbaSizes[i] && a->rgbaOffsets[i] == b->rgbaOffsets[i]) & (unsigned char)match;
    }
    return match;
}

unsigned int grGetPackedColorWriteMask(_grSurfaceDescriptor *desc, unsigned int colorMask)
{
    int r, g, b, a;

    GR_UTIL_SMALL_CHECK(desc != NULL, "grUtilSmall.inl", 322);
    a = (unsigned char)hgBitMask32((int)colorMask, 3);
    b = (unsigned char)hgBitMask32((int)colorMask, 2);
    g = (unsigned char)hgBitMask32((int)colorMask, 1);
    r = (unsigned char)hgBitMask32((int)colorMask, 0);
    return grPackColor(grSetRGBA(r, g, b, a), desc);
}

unsigned int grUnpackColor(unsigned int color, _grSurfaceDescriptor *desc)
{
    /* unsigned char like the decompile: hgBitMask32's -1 truncates to 255 */
    unsigned char comp[4];
    int i;

    for (i = 0; i < 4; ++i)
    {
        int offset = grGetDescriptorOffset(desc, i);
        int size = grGetDescriptorSize(desc, i);
        int c = ((1 << size) - 1) & (int)(color >> offset);
        GR_UTIL_SMALL_CHECK(hgInRange32(offset, 0, 31), "grUtilSmall.inl", 349);
        GR_UTIL_SMALL_CHECK(hgInRange32(size, 0, 15), "grUtilSmall.inl", 350);
        GR_UTIL_SMALL_CHECK(size + offset <= 32, "grUtilSmall.inl", 351);
        if (size != 0)
        {
            if (hgInRange32(size, 4, 8) != 0)
            {
                /* replicate to a full byte: high part | low part */
                c = (c << (8 - size) >> size) | (c << (8 - size));
            }
            else if (hgInRange32(size, 9, 15) != 0)
            {
                c >>= size - 8;
            }
            else
            {
                GR_UTIL_SMALL_CHECK(hgInRange32(size, 1, 3), "grUtilSmall.inl", 367);
                switch (size)
                {
                case 1:
                    /* hgBitMask32(c,0) is 0 or -1; unsigned char store -> 0/255 */
                    c = hgBitMask32(c, 0);
                    break;
                case 2:
                    c = (16 * ((4 * c) | c)) | (4 * c) | c;
                    break;
                case 3:
                    c = (c >> 1) | (32 * c) | (4 * c);
                    break;
                default:
                    grAssert("0", "grUtilSmall.inl", 373);
                    grDie(0);
                }
            }
        }
        else
        {
            /* zero-width channel: alpha defaults to 255, others to 0 */
            c = (i == 3) ? 255 : 0;
        }
        comp[i] = c;
    }
    return grSetRGBA(comp[0], comp[1], comp[2], comp[3]);
}

/* --- g_orderedDitherKernel (DLL data, libGLES_CM.dll.c:2389) --------------- */
int g_orderedDitherKernel[4] = { -7803290, 1140859562, -581579657, 295252923 };

/* --- grDitherColor (grUtilSmall.inl 273; DLL 00439440) --------------------- */

unsigned int grDitherColor(unsigned int color, _grSurfaceDescriptor *desc, int x, int y)
{
    int na[9];
    int k;

    k = (int)((unsigned int)hgRor32(g_orderedDitherKernel[y & 3], 8 * (x & 3)) >> 24);
    na[7] = grUnpackRGBA(color, GR_COLOR_R);
    na[6] = grUnpackRGBA(color, GR_COLOR_G);
    na[5] = grUnpackRGBA(color, GR_COLOR_B);
    na[4] = grUnpackRGBA(color, GR_COLOR_A);
    na[3] = desc->rgbaSizes[0];
    na[2] = desc->rgbaSizes[1];
    na[1] = desc->rgbaSizes[2];
    na[0] = desc->rgbaSizes[3];
    na[7] = na[7] - (na[7] >> na[3]) + (k >> na[3]);
    na[6] = na[6] - (na[6] >> na[2]) + (k >> na[2]);
    na[5] = na[5] - (na[5] >> na[1]) + (k >> na[1]);
    na[4] = na[4] - (na[4] >> na[0]) + (k >> na[0]);
    return grPackColor(grSetRGBA(na[7], na[6], na[5], na[4]), desc);
}
