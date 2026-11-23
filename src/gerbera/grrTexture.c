/* grrTexture.c - texture format descriptors, mipmap allocation, mipmap
 * generation, and paletted texture upload (grrTexture.inl).
 *
 * Assert sites keep the original grrTexture.inl file/line strings.
 */
#include "gerbera/grState.h"
#include "gerbera/grRGBA.h"
#include "hg/hgInt32.h"
#include "hg/hgMemory.h"

/* --- s_bpp (data.fragments.c; verified by grrSelfTest) --------------------- */

static const unsigned char s_bpp[18] =
{
    8, 32, 32, 8, 16,
    4, 4, 4, 4, 4,
    8, 8, 8, 8, 8,
    16, 16, 16
};

/* --- s_infos (data.fragments.c 3351; DLL 37645) ---------------------------- */

static const _grTextureFormatInfo s_infos[18] =
{
    { 1, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0 },      /* ALPHA */
    { 4, 8, 8, 8, 0, 16, 8, 0, 0, 4, 0, 0 },     /* RGB */
    { 4, 8, 8, 8, 8, 16, 8, 0, 24, 5, 0, 0 },    /* RGBA */
    { 1, 8, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },      /* LUMINANCE */
    { 2, 8, 0, 0, 8, 0, 0, 0, 8, 2, 0, 0 },      /* LUMINANCE_ALPHA */
    { 4, 8, 8, 8, 0, 0, 8, 16, 0, 4, 1, 0 },     /* pal4/RGB8 */
    { 4, 8, 8, 8, 8, 0, 8, 16, 24, 5, 1, 0 },    /* pal4/RGBA8 */
    { 2, 5, 6, 5, 0, 11, 5, 0, 0, 4, 1, 0 },     /* pal4/RGB565 */
    { 2, 4, 4, 4, 4, 12, 8, 4, 0, 5, 1, 0 },     /* pal4/RGBA4 */
    { 2, 5, 5, 5, 1, 11, 6, 1, 0, 5, 1, 0 },     /* pal4/RGBA5551 */
    { 4, 8, 8, 8, 0, 0, 8, 16, 0, 4, 2, 0 },     /* pal8/RGB8 */
    { 4, 8, 8, 8, 8, 0, 8, 16, 24, 5, 2, 0 },    /* pal8/RGBA8 */
    { 2, 5, 6, 5, 0, 11, 5, 0, 0, 4, 2, 0 },     /* pal8/RGB565 */
    { 2, 4, 4, 4, 4, 12, 8, 4, 0, 5, 2, 0 },     /* pal8/RGBA4 */
    { 2, 5, 5, 5, 1, 11, 6, 1, 0, 5, 2, 0 },     /* pal8/RGBA5551 */
    { 2, 5, 6, 5, 0, 11, 5, 0, 0, 4, 0, 0 },     /* RGB565 */
    { 2, 4, 4, 4, 4, 8, 4, 0, 12, 5, 0, 0 },     /* RGBA4444 */
    { 2, 5, 5, 5, 1, 10, 5, 0, 15, 5, 0, 0 }     /* RGBA5551 */
};

int grrGetBitsPerPixel(int texFormat)
{
    if (hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX - 1) == 0 &&
        grAssert("hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX-1)",
                 "grrTexture.inl", 76) != 0)
        grDie(0);
    return s_bpp[texFormat];
}

void grrGetTextureFormatDescriptor(_grSurfaceDescriptor *desc, int texFormat)
{
    const _grTextureFormatInfo *info;
    unsigned char colorMask;

    info = &s_infos[texFormat];
    if (desc == NULL &&
        grAssert("desc", "grrTexture.inl", 132) != 0)
        grDie(0);
    if (hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX - 1) == 0 &&
        grAssert("hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX-1)",
                 "grrTexture.inl", 133) != 0)
        grDie(0);
    grClearDescriptor(desc);
    colorMask = (unsigned char)((info->rSize != 0) |
                                ((info->gSize != 0) << 1) |
                                ((info->bSize != 0) << 2) |
                                ((info->aSize != 0) << 3));
    grSetDescriptorSizes(desc, info->rSize, info->gSize, info->bSize,
                         info->aSize);
    grSetDescriptorOffsets(desc, info->rOffset, info->gOffset,
                           info->bOffset, info->aOffset);
    desc->colorBytes = info->colorBytes;
    desc->colorMask = colorMask;
    desc->pixelFormat = info->pixelFormat;
    desc->paletteFormat = info->paletteFormat;
}

void grrSelfTest(void)
{
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_ALPHA8) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_ALPHA8) == 8", "grrTexture.inl", 159) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_LUMINANCE8) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_LUMINANCE8) == 8", "grrTexture.inl", 160) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_LUMINANCE8_ALPHA8) != 16 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_LUMINANCE8_ALPHA8) == 16", "grrTexture.inl", 161) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGB8) != 32 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGB8) == 32", "grrTexture.inl", 162) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGBA8) != 32 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGBA8) == 32", "grrTexture.inl", 163) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGB8) != 4 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGB8) == 4", "grrTexture.inl", 164) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGBA8) != 4 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGBA8) == 4", "grrTexture.inl", 165) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGB565) != 4 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGB565) == 4", "grrTexture.inl", 166) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGBA4) != 4 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGBA4) == 4", "grrTexture.inl", 167) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGBA5551) != 4 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE4_RGBA5551) == 4", "grrTexture.inl", 168) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGB8) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGB8) == 8", "grrTexture.inl", 169) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGBA8) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGBA8) == 8", "grrTexture.inl", 170) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGB565) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGB565) == 8", "grrTexture.inl", 171) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGBA4) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGBA4) == 8", "grrTexture.inl", 172) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGBA5551) != 8 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_PALETTE8_RGBA5551) == 8", "grrTexture.inl", 173) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGB565) != 16 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGB565) == 16", "grrTexture.inl", 174) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGBA4444) != 16 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGBA4444) == 16", "grrTexture.inl", 175) != 0)
        grDie(0);
    if (grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGBA5551) != 16 &&
        grAssert("grrGetBitsPerPixel(GR_TEXTUREFORMAT_RGBA5551) == 16", "grrTexture.inl", 176) != 0)
        grDie(0);
}

int grrSetupTextureLevel(_grTexture *texture, int mipLevel, int width, int height, int texFormat)
{
    _grMipLevel *mipmap;
    int bitsPerPixel;
    unsigned int allocSize;
    void *pixels;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 206) != 0)
        grDie(0);
    if (hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS - 1) == 0 &&
        grAssert("hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS-1)", "grrTexture.inl", 207) != 0)
        grDie(0);
    if (!(hgInRange32(width, 0, GR_MAX_TEXTURE_DIMENSION) != 0 && hgIsPowerOfTwo32(width) != 0) &&
        grAssert("hgInRange32(width, 0, GR_MAX_TEXTURE_DIMENSION) && hgIsPowerOfTwo32(width)",
                 "grrTexture.inl", 208) != 0)
        grDie(0);
    if (!(hgInRange32(height, 0, GR_MAX_TEXTURE_DIMENSION) != 0 && hgIsPowerOfTwo32(height) != 0) &&
        grAssert("hgInRange32(height, 0, GR_MAX_TEXTURE_DIMENSION) && hgIsPowerOfTwo32(height)",
                 "grrTexture.inl", 209) != 0)
        grDie(0);
    if (hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX - 1) == 0 &&
        grAssert("hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX-1)", "grrTexture.inl", 210) != 0)
        grDie(0);

    mipmap = &texture->mipmap[mipLevel];
    bitsPerPixel = grrGetBitsPerPixel(texFormat);
    allocSize = (unsigned int)hgMax32(1, (bitsPerPixel * height * width) >> 3);
    pixels = NULL;
    if (height * width != 0)
    {
        if (allocSize == 0 &&
            grAssert("allocSize", "grrTexture.inl", 220) != 0)
            grDie(0);
        pixels = eglAlloc(allocSize);
        if (pixels == NULL)
            return 0;
    }
    if (mipmap->pbuffer == 0)
        eglFree(mipmap->pixels);
    grrReplaceTextureLevel(texture, mipLevel, pixels, width, height, texFormat);
    return 1;
}

void grrReplaceTextureLevel(_grTexture *texture, int mipLevel, void *pixels,
                            int width, int height, int texFormat)
{
    _grMipLevel *mipmap;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 238) != 0)
        grDie(0);
    if (hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS - 1) == 0 &&
        grAssert("hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS-1)", "grrTexture.inl", 239) != 0)
        grDie(0);
    if (!(hgInRange32(width, 0, GR_MAX_TEXTURE_DIMENSION) != 0 && hgIsPowerOfTwo32(width) != 0) &&
        grAssert("hgInRange32(width, 0, GR_MAX_TEXTURE_DIMENSION) && hgIsPowerOfTwo32(width)",
                 "grrTexture.inl", 240) != 0)
        grDie(0);
    if (!(hgInRange32(height, 0, GR_MAX_TEXTURE_DIMENSION) != 0 && hgIsPowerOfTwo32(height) != 0) &&
        grAssert("hgInRange32(height, 0, GR_MAX_TEXTURE_DIMENSION) && hgIsPowerOfTwo32(height)",
                 "grrTexture.inl", 241) != 0)
        grDie(0);
    if (hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX - 1) == 0 &&
        grAssert("hgInRange32(texFormat, 0, GR_TEXTUREFORMAT_MAX-1)", "grrTexture.inl", 242) != 0)
        grDie(0);

    mipmap = &texture->mipmap[mipLevel];
    mipmap->pixels = pixels;
    mipmap->log2Dim[0] = (short)(31 - hgClz32((unsigned int)width));
    mipmap->log2Dim[1] = (short)(31 - hgClz32((unsigned int)height));
    mipmap->texFormat = (unsigned short)texFormat;
    mipmap->initialized = 1;
    texture->parmMask |= 1u;
}

int grrCalcTextureMipmaps(Gerbera_s *state, _grTexture *texture)
{
    int log2Dim[2];
    void *success;
    int baseWidth;
    int baseHeight;
    int numMipmaps;
    int level;
    grBlitImage_s srcImg;
    grBlitImage_s dstImg;
    grBlitInfo_s blitInfo;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 258) != 0)
        grDie(0);
    success = grGetTextureMipLevel(texture, 0, log2Dim);
    if (success == NULL &&
        grAssert("success", "grrTexture.inl", 266) != 0)
        grDie(0);

    baseWidth = 1 << log2Dim[0];
    baseHeight = 1 << log2Dim[1];
    numMipmaps = hgMax32(log2Dim[0], log2Dim[1]) + 1;
    if (hgInRange32(numMipmaps, 1, GR_MAX_TEXTURE_MIPMAPS) == 0 &&
        grAssert("hgInRange32(numMipmaps, 1, GR_MAX_TEXTURE_MIPMAPS)",
                 "grrTexture.inl", 282) != 0)
        grDie(0);

    hgMemSet(&blitInfo, 0, (unsigned int)sizeof(blitInfo));
    blitInfo.srcImage = &srcImg;
    blitInfo.dstImage = &dstImg;

    for (level = 1; level < numMipmaps; ++level)
    {
        int w = hgMax32(baseWidth >> level, 1);
        int h = hgMax32(baseHeight >> level, 1);
        int fmt = texture->mipmap[0].texFormat;

        if (grrSetupTextureLevel(texture, level, w, h, fmt) == 0)
            return 0;

        hgMemSet(&srcImg, 0, (unsigned int)sizeof(srcImg));
        hgMemSet(&dstImg, 0, (unsigned int)sizeof(dstImg));
        srcImg.type = GR_BLITIMAGE_TEXTURE;
        srcImg.texture.texture = texture;
        srcImg.texture.mipLevel = level - 1;
        dstImg.type = GR_BLITIMAGE_TEXTURE;
        dstImg.texture.texture = texture;
        dstImg.texture.mipLevel = level;
        grrBlitMipmap(state, &blitInfo);
    }
    return 1;
}

void grrFreeTextureLevel(_grTexture *texture, int mipLevel)
{
    _grMipLevel *mipmap;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 341) != 0)
        grDie(0);
    if (hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS - 1) == 0 &&
        grAssert("hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS-1)",
                 "grrTexture.inl", 342) != 0)
        grDie(0);
    mipmap = &texture->mipmap[mipLevel];
    if (mipmap->pbuffer == 0)
        eglFree(mipmap->pixels);
    mipmap->pixels = NULL;
    mipmap->log2Dim[0] = -1;
    mipmap->log2Dim[1] = -1;
    mipmap->texFormat = GR_TEXTUREFORMAT_MAX;
    mipmap->initialized = 0;
    mipmap->pbuffer = 0;
    texture->parmMask |= 1u;
}

void grrFreeTextureData(_grTexture *texture)
{
    int i;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 370) != 0)
        grDie(0);
    texture->parmMask |= 1u;
    for (i = 0; i < GR_MAX_TEXTURE_MIPMAPS; ++i)
        grrFreeTextureLevel(texture, i);
    eglFree(texture->palette);
    texture->palette = NULL;
}

int grrSetPalettedTexture(_grTexture *texture, _grSurfaceDescriptor *inDesc, int texFormat,
                          int width, int height, int numLevels, void *data)
{
    const unsigned char *srcPtr;
    int entryBytes;
    int is4Bit;
    int numEntries;
    int paletteBytes;
    _grSurfaceDescriptor descriptor;
    int i;

    if (!(texture != NULL && inDesc != NULL && data != NULL) &&
        grAssert("texture && inDesc && data", "grrTexture.inl", 406) != 0)
        grDie(0);
    if (hgInRange32(width, 0, GR_MAX_TEXTURE_DIMENSION) == 0 &&
        grAssert("hgInRange32(width, 0, GR_MAX_TEXTURE_DIMENSION)", "grrTexture.inl", 407) != 0)
        grDie(0);
    if (hgInRange32(height, 0, GR_MAX_TEXTURE_DIMENSION) == 0 &&
        grAssert("hgInRange32(height, 0, GR_MAX_TEXTURE_DIMENSION)", "grrTexture.inl", 408) != 0)
        grDie(0);
    if (hgInRange32(numLevels, 1, GR_MAX_TEXTURE_MIPMAPS) == 0 &&
        grAssert("hgInRange32(numLevels, 1, GR_MAX_TEXTURE_MIPMAPS)", "grrTexture.inl", 409) != 0)
        grDie(0);
    if (!(inDesc->paletteFormat == GR_PALETTEFORMAT_4BIT ||
          inDesc->paletteFormat == GR_PALETTEFORMAT_8BIT) &&
        grAssert("inDesc->paletteFormat == GR_PALETTEFORMAT_4BIT || inDesc->paletteFormat == GR_PALETTEFORMAT_8BIT",
                 "grrTexture.inl", 411) != 0)
        grDie(0);

    texture->parmMask |= 1u;
    srcPtr = (const unsigned char *)data;
    entryBytes = inDesc->colorBytes;
    is4Bit = (inDesc->paletteFormat == GR_PALETTEFORMAT_4BIT);
    numEntries = is4Bit ? 16 : 256;
    paletteBytes = entryBytes * numEntries;

    grrGetTextureFormatDescriptor(&descriptor, texFormat);
    eglFree(texture->palette);
    texture->palette = eglAlloc((unsigned int)paletteBytes);
    if (texture->palette == NULL)
        return 0;

    if (entryBytes != 4 || inDesc->rgbaSizes[3] != 0)
    {
        if (grDescriptorsEqual(inDesc, &descriptor) == 0 &&
            grAssert("grDescriptorsEqual(inDesc, &descriptor)", "grrTexture.inl", 448) != 0)
            grDie(0);
        hgMemCopy(texture->palette, srcPtr, (unsigned int)paletteBytes);
        srcPtr += paletteBytes;
    }
    else
    {
        unsigned int *dstPal = (unsigned int *)texture->palette;
        for (i = 0; i < numEntries; ++i)
        {
            unsigned int rgba = grSetRGBA(srcPtr[0], srcPtr[1], srcPtr[2], 0);
            srcPtr += 3;
            dstPal[i] = grPackColor(rgba, &descriptor);
        }
    }

    for (i = 0; i < numLevels; ++i)
    {
        int levelBytes = hgMax32((height * width) >> (is4Bit != 0), 1);
        if (grrSetupTextureLevel(texture, i, width, height, texFormat) == 0)
            return 0;
        if (width != 0 && height != 0)
        {
            int dummyDim[2];
            void *dstPixels = grGetTextureMipLevel(texture, i, dummyDim);
            width = hgMax32(width >> 1, 1);
            height = hgMax32(height >> 1, 1);
            hgMemCopy(dstPixels, srcPtr, (unsigned int)levelBytes);
            srcPtr += levelBytes;
        }
    }
    return 1;
}
