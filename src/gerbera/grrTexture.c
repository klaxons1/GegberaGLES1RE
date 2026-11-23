/* grrTexture.c - texture format descriptors (port of the decompiled
 * grrTexture.inl). The self-test helpers (grrSelfTest, grrGetBitsPerPixel)
 * land with the grr-module session. */
#include "gerbera/grTexture.h"
#include "hg/hgInt32.h"

/* --- s_infos (data.fragments.c 3351; DLL 37645) --------------------------------
 * Per-GR_TEXTUREFORMAT row: colorBytes, r/g/b/a sizes, r/g/b/a offsets,
 * pixelFormat, paletteFormat (0 = direct, 1 = RGB565 palette, 2 = RGBA4444
 * palette). Transcribed verbatim. */

static const _grTextureFormatInfo s_infos[18] =
{
    { 1, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0 },      /* ALPHA */
    { 4, 8, 8, 8, 0, 16, 8, 0, 0, 4, 0, 0 },     /* RGB */
    { 4, 8, 8, 8, 8, 16, 8, 0, 24, 5, 0, 0 },    /* RGBA */
    { 1, 8, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },      /* LUMINANCE */
    { 2, 8, 0, 0, 8, 0, 0, 0, 8, 2, 0, 0 },      /* LUMINANCE_ALPHA */
    { 4, 8, 8, 8, 0, 0, 8, 16, 0, 4, 1, 0 },     /* pal 565/RGB8888 */
    { 4, 8, 8, 8, 8, 0, 8, 16, 24, 5, 1, 0 },    /* pal 565/RGBA8888 */
    { 2, 5, 6, 5, 0, 11, 5, 0, 0, 4, 1, 0 },     /* pal 565/RGB565 */
    { 2, 4, 4, 4, 4, 12, 8, 4, 0, 5, 1, 0 },     /* pal 565/RGBA4444 */
    { 2, 5, 5, 5, 1, 11, 6, 1, 0, 5, 1, 0 },     /* pal 565/RGBA5551 */
    { 4, 8, 8, 8, 0, 0, 8, 16, 0, 4, 2, 0 },     /* pal 4444/RGB8888 */
    { 4, 8, 8, 8, 8, 0, 8, 16, 24, 5, 2, 0 },    /* pal 4444/RGBA8888 */
    { 2, 5, 6, 5, 0, 11, 5, 0, 0, 4, 2, 0 },     /* pal 4444/RGB565 */
    { 2, 4, 4, 4, 4, 12, 8, 4, 0, 5, 2, 0 },     /* pal 4444/RGBA4444 */
    { 2, 5, 5, 5, 1, 11, 6, 1, 0, 5, 2, 0 },     /* pal 4444/RGBA5551 */
    { 2, 5, 6, 5, 0, 11, 5, 0, 0, 4, 0, 0 },     /* RGB565 */
    { 2, 4, 4, 4, 4, 8, 4, 0, 12, 5, 0, 0 },     /* RGBA4444 */
    { 2, 5, 5, 5, 1, 10, 5, 0, 15, 5, 0, 0 }     /* RGBA5551 */
};

/* --- grrGetTextureFormatDescriptor (grrTexture.inl 131; DLL 37602-37643) ------- */

void grrGetTextureFormatDescriptor(_grSurfaceDescriptor *desc, int texFormat)
{
    const _grTextureFormatInfo *info;
    unsigned char colorMask;

    info = &s_infos[texFormat];
    if (desc == NULL &&
        grAssert("desc", "grrTexture.inl", 132) != 0)
        grDie(0);
    if ((hgInRange32(texFormat, 0, 17) == 0) &&
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

/* --- grrFreeTextureLevel / grrFreeTextureData (grrTexture.inl 341 / 370) --- */

extern void eglFree(void *ptr);

void grrFreeTextureLevel(_grTexture *texture, int mipLevel)
{
    _grMipLevel *mipmap;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 341) != 0)
        grDie(0);
    if ((hgInRange32(mipLevel, 0, GR_MAX_TEXTURE_MIPMAPS - 1) == 0) &&
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
    texture->flags183 |= 1u;
}

void grrFreeTextureData(_grTexture *texture)
{
    int i;

    if (texture == NULL &&
        grAssert("texture", "grrTexture.inl", 370) != 0)
        grDie(0);
    texture->parmMask |= 1u;
    texture->flags183 |= 1u;
    for (i = 0; i < GR_MAX_TEXTURE_MIPMAPS; ++i)
        grrFreeTextureLevel(texture, i);
    eglFree(texture->palette);
    texture->palette = NULL;
}
