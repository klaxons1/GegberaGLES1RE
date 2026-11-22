/* grTexture.c - texture object functions (from the decompiled grApiSmall.inl
 * texture block + grUtilSmall.inl grIsValidTexture/grUpdateTexture).
 *
 * grSetActiveTexture needs Gerbera_s (state->textureUnit) and lands with the
 * driver-context port.
 */
#include "gerbera/grTexture.h"
#include "hg/hgInt32.h"
#include "hg/hgMemory.h"

int grGetTextureTargetDimensionality(int texTarget)
{
    GR_TEXTURE_CHECK(texTarget == GR_TEXTURETARGET_2D, "grUtil.h", 191);
    return 2;
}

int grIsPalettedTextureFormat(int texFormat)
{
    return hgInRange32(texFormat, GR_TEXTUREFORMAT_PALETTED_FIRST, GR_TEXTUREFORMAT_PALETTED_LAST);
}

int grIsValidTexture(_grTexture *texture)
{
    GR_TEXTURE_CHECK(texture != NULL, "grUtilSmall.inl", 68);
    GR_TEXTURE_CHECK(hgInRange32(texture->target, 0, GR_TEXTURETARGET_MAX), "grUtilSmall.inl", 70);
    GR_TEXTURE_CHECK(hgInRange32(texture->wrapMode[0], 0, GR_WRAPMODE_MAX - 1), "grUtilSmall.inl", 71);
    GR_TEXTURE_CHECK(hgInRange32(texture->wrapMode[1], 0, GR_WRAPMODE_MAX - 1), "grUtilSmall.inl", 72);
    GR_TEXTURE_CHECK(hgInRange32(texture->numMipLevels, 0, GR_MAX_TEXTURE_MIPMAPS), "grUtilSmall.inl", 73);
    GR_TEXTURE_CHECK(hgInRange32(texture->filterMask[GR_FILTERTYPE_MIN], 0, GR_FILTERMASK_ALL), "grUtilSmall.inl", 74);
    GR_TEXTURE_CHECK(hgInRange32(texture->filterMask[GR_FILTERTYPE_MAG], 0, GR_FILTERMASK_ALL), "grUtilSmall.inl", 75);
    GR_TEXTURE_CHECK(texture->genMipMaps == 0 || texture->genMipMaps == 1, "grUtilSmall.inl", 77);
    return 1;
}

void grUpdateTexture(_grTexture *texture)
{
    /* locals mirror the decompile's parmMask[] slots */
    int texFormat0;             /* [14] */
    int havePalette;            /* [13] */
    int chainable;              /* [12] */
    int square;                 /* [11] */
    int maxLevel;               /* [10] */
    int expDim[2];              /* [17..18] */

    GR_TEXTURE_CHECK(texture != NULL, "grUtilSmall.inl", 113);
    GR_TEXTURE_CHECK(grIsValidTexture(texture) != 0, "grUtilSmall.inl", 114);

    texFormat0 = texture->mipmap[0].texFormat;
    havePalette = 1;
    chainable = 1;
    square = 1;
    maxLevel = 0;
    texture->parmMask = 2;
    if (grGetTextureMipLevel(texture, 0, expDim) != NULL)
    {
        int filterMin;
        int baseDim[2];
        unsigned int parmMaskOut;

        if (grIsPalettedTextureFormat(texFormat0) != 0)
            havePalette = (texture->palette != NULL);
        baseDim[0] = texture->mipmap[0].log2Dim[0];
        baseDim[1] = texture->mipmap[0].log2Dim[1];
        maxLevel = hgMax32(baseDim[0], baseDim[1]);
        if (baseDim[0] != baseDim[1])
            square = 0;
        GR_TEXTURE_CHECK(hgInRange32(maxLevel, 0, GR_MAX_TEXTURE_MIPMAPS), "grUtilSmall.inl", 159);
        if ((texture->filterMask[GR_FILTERTYPE_MIN] & 2) != 0)
        {
            int level, c;
            for (level = 0; level <= maxLevel; ++level)
            {
                int actDim[2];
                void *pixels = grGetTextureMipLevel(texture, level, actDim);
                if (pixels != NULL && texFormat0 == texture->mipmap[level].texFormat)
                {
                    for (c = 0; c < 2; ++c)
                    {
                        GR_TEXTURE_CHECK(actDim[c] != -1, "grUtilSmall.inl", 183);
                        if (expDim[c] != actDim[c])
                            chainable = 0;
                        expDim[c] = hgMax32(expDim[c] - 1, 0);
                    }
                }
                else
                {
                    chainable = 0;
                }
            }
        }
        parmMaskOut = 2;
        if (square != 0)
            parmMaskOut |= 0x10u;
        if (havePalette != 0 && chainable != 0)
        {
            parmMaskOut |= 8u;
            texture->numMipLevels = (unsigned char)(maxLevel + 1);
        }
        filterMin = texture->filterMask[GR_FILTERTYPE_MIN];
        if ((filterMin & 2) == 0 || chainable != 0)
        {
            if (filterMin != texture->filterMask[GR_FILTERTYPE_MAG])
                parmMaskOut |= 0x20u;
            texture->parmMask = parmMaskOut | 4u;
        }
    }
}

void grInitTexture(_grTexture *texture)
{
    int i;

    GR_TEXTURE_CHECK(texture != NULL, "grApiSmall.inl", 1240);
    texture->parmMask = 3;
    texture->filterMask[0] = 6;
    texture->filterMask[1] = 1;
    for (i = 0; i < 2; ++i)
        texture->wrapMode[i] = 0;
    texture->target = GR_TEXTURETARGET_MAX;   /* "unbound" sentinel (init writes 1) */
    texture->numMipLevels = 1;
    texture->palette = NULL;
    for (i = 0; i < GR_MAX_TEXTURE_MIPMAPS; ++i)
    {
        _grMipLevel *mipmap = &texture->mipmap[i];
        mipmap->pixels = NULL;
        mipmap->log2Dim[0] = -1;
        mipmap->log2Dim[1] = -1;
        mipmap->texFormat = GR_TEXTUREFORMAT_MAX;
        mipmap->initialized = 0;
        mipmap->pbuffer = 0;
    }
    grSetRectangle(&texture->cropRect, 0, 0, 0, 0);
    texture->genMipMaps = 0;
    GR_TEXTURE_CHECK(grIsValidTexture(texture) != 0, "grApiSmall.inl", 1281);
}

int grGetTextureTarget(_grTexture *texture)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1130);
    GR_TEXTURE_CHECK(hgInRange32(texture->target, 0, GR_TEXTURETARGET_MAX), "grApiSmall.inl", 1131);
    return texture->target;
}

void *grGetTextureMipLevel(_grTexture *texture, int level, int *log2Dim)
{
    _grMipLevel *mipmap;
    int dimensionality;
    int i;

    GR_TEXTURE_CHECK(log2Dim != NULL, "grApiSmall.inl", 1137);
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1138);
    GR_TEXTURE_CHECK(hgInRange32(level, 0, GR_MAX_TEXTURE_MIPMAPS - 1), "grApiSmall.inl", 1139);
    mipmap = &texture->mipmap[level];
    if (mipmap->pixels == NULL)
        return NULL;
    dimensionality = grGetTextureTargetDimensionality(texture->target);
    for (i = 0; i < dimensionality; ++i)
    {
        GR_TEXTURE_CHECK(hgInRange32(mipmap->log2Dim[i], 0, GR_MAX_TEXTURE_DIMENSION_LOG2), "grApiSmall.inl", 1151);
        log2Dim[i] = mipmap->log2Dim[i];
    }
    return mipmap->pixels;
}

int grGetTextureMipLevelFormat(_grTexture *texture, int level)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1161);
    GR_TEXTURE_CHECK(hgInRange32(level, 0, GR_MAX_TEXTURE_MIPMAPS - 1), "grApiSmall.inl", 1162);
    GR_TEXTURE_CHECK(hgInRange32(texture->mipmap[level].texFormat, 0, GR_TEXTUREFORMAT_MAX - 1), "grApiSmall.inl", 1166);
    return texture->mipmap[level].texFormat;
}

int grTextureMipLevelInitialized(_grTexture *texture, int level)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1173);
    GR_TEXTURE_CHECK(hgInRange32(level, 0, GR_MAX_TEXTURE_MIPMAPS - 1), "grApiSmall.inl", 1174);
    return texture->mipmap[level].initialized;
}

void grSetTextureTarget(_grTexture *texture, int texTarget)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1183);
    GR_TEXTURE_CHECK(hgInRange32(texTarget, 0, GR_TEXTURETARGET_MAX - 1), "grApiSmall.inl", 1184);
    if (texture->target != texTarget)
    {
        texture->target = (unsigned char)texTarget;
        texture->parmMask |= 1u;
    }
}

void grSetTextureWrapMode(_grTexture *texture, int coord, int wrapMode)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1302);
    GR_TEXTURE_CHECK(hgInRange32(coord, 0, GR_TEXCOORD_MAX - 1), "grApiSmall.inl", 1303);
    if (wrapMode != texture->wrapMode[coord])
    {
        texture->wrapMode[coord] = (unsigned char)wrapMode;
        texture->parmMask |= 1u;
    }
}

unsigned char grGetTextureWrapMode(_grTexture *texture, int coord)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1315);
    GR_TEXTURE_CHECK(hgInRange32(coord, 0, GR_TEXCOORD_MAX - 1), "grApiSmall.inl", 1316);
    return texture->wrapMode[coord];
}

void grSetTextureGenMipmaps(_grTexture *texture, int genMipmaps)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1323);
    GR_TEXTURE_CHECK(genMipmaps == 1 || genMipmaps == 0, "grApiSmall.inl", 1324);
    if (texture->genMipMaps != genMipmaps)
        texture->genMipMaps = genMipmaps;
}

int grGetTextureGenMipmaps(_grTexture *texture)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1334);
    return texture->genMipMaps;
}

void grSetTextureCropRect(_grTexture *texture, _grRect *rect)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1341);
    GR_TEXTURE_CHECK(rect != NULL, "grApiSmall.inl", 1342);
    hgMemCopy(&texture->cropRect, rect, 0x10u);
}

void grGetTextureCropRect(_grTexture *texture, _grRect *rect)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1352);
    GR_TEXTURE_CHECK(rect != NULL, "grApiSmall.inl", 1353);
    hgMemCopy(rect, &texture->cropRect, 0x10u);
}

void grSetTextureFilter(_grTexture *texture, int filterType, unsigned int filterMask)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1364);
    GR_TEXTURE_CHECK(hgInRange32(filterType, 0, GR_FILTERTYPE_MAG), "grApiSmall.inl", 1365);
    GR_TEXTURE_CHECK((filterMask & 0xFFFFFFF8u) == 0, "grApiSmall.inl", 1366);
    if (filterType == GR_FILTERTYPE_MAG)
    {
        GR_TEXTURE_CHECK((filterMask & (GR_FILTERMASK_USE_MIPMAPS | GR_FILTERMASK_FILTER_MIPMAPS)) == 0,
                         "grApiSmall.inl", 1371);
    }
    if ((int)filterMask != texture->filterMask[filterType])
    {
        texture->filterMask[filterType] = (unsigned char)filterMask;
        texture->parmMask |= 1u;
    }
}

unsigned int grGetTextureFilter(_grTexture *texture, int filterType)
{
    GR_TEXTURE_CHECK(texture != NULL && grIsValidTexture(texture) != 0, "grApiSmall.inl", 1383);
    GR_TEXTURE_CHECK(hgInRange32(filterType, 0, GR_FILTERTYPE_MAG), "grApiSmall.inl", 1384);
    return texture->filterMask[filterType];
}
