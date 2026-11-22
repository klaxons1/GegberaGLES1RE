/* grTexture.h - texture objects (port of the decompiled texture parts of
 * grApiSmall.inl / grUtilSmall.inl / grSmall.c; struct cv_anon_1477 /
 * cv_anon_1472 in the decompile).
 *
 * Enum values pinned from the decompile:
 *  - GR_TEXTURETARGET: 2D = 0, MAX = 1 (MAX doubles as the "unbound"
 *    sentinel; grInitTexture stores target = GR_TEXTURETARGET_MAX and
 *    oglUtil tests `oldTexTarget == GR_TEXTURETARGET_MAX` to detect the
 *    first bind).
 *  - GR_TEXTUREFORMAT: oglMapInternalTextureFormat maps GL_ALPHA..GL_LUMINANCE_ALPHA
 *    (6406..6410) onto 0..4; paletted formats are 5..14 (grIsPalettedTextureFormat);
 *    15..17 unnamed so far; grInitTexture seeds texFormat = 18 = MAX.
 *  - GR_MAX_TEXTURE_MIPMAPS = 11 (level asserts 0..10, count asserts 0..11,
 *    grInitTexture zeroes 11 mip levels).
 *  - GR_FILTERMASK_ALL = 7 (filterMask range asserts), filterMask init = {6, 1}.
 *  - GR_WRAPMODE_MAX = 3 (wrapMode asserts 0..2), init = 0.
 *  - GR_MAX_TEXTURE_UNITS = 2 (grSetActiveTexture/grSetActiveTextureClient).
 */
#ifndef GR_TEXTURE_H
#define GR_TEXTURE_H

#include "hg/hgDefs.h"
#include "gerbera/grDefs.h"

HG_EXTERN_C_BEGIN

enum GR_TEXTURETARGET_e
{
    GR_TEXTURETARGET_2D = 0,
    GR_TEXTURETARGET_MAX = 1      /* also the "unbound" sentinel */
};

enum GR_TEXTUREFORMAT_e
{
    GR_TEXTUREFORMAT_ALPHA = 0,
    GR_TEXTUREFORMAT_RGB = 1,
    GR_TEXTUREFORMAT_RGBA = 2,
    GR_TEXTUREFORMAT_LUMINANCE = 3,
    GR_TEXTUREFORMAT_LUMINANCE_ALPHA = 4,
    /* 5..14: paletted formats (grIsPalettedTextureFormat range) */
    GR_TEXTUREFORMAT_PALETTED_FIRST = 5,
    GR_TEXTUREFORMAT_PALETTED_LAST = 14,
    /* 15..17: formats without pinned names yet */
    GR_TEXTUREFORMAT_15 = 15,
    GR_TEXTUREFORMAT_16 = 16,
    GR_TEXTUREFORMAT_17 = 17,
    GR_TEXTUREFORMAT_MAX = 18
};

enum GR_WRAPMODE_e
{
    GR_WRAPMODE_0 = 0,
    GR_WRAPMODE_1 = 1,
    GR_WRAPMODE_2 = 2,
    GR_WRAPMODE_MAX = 3
};

enum GR_FILTERTYPE_e
{
    GR_FILTERTYPE_MIN = 0,
    GR_FILTERTYPE_MAG = 1
};

#define GR_FILTERMASK_FILTER 1
#define GR_FILTERMASK_USE_MIPMAPS 2
#define GR_FILTERMASK_FILTER_MIPMAPS 4
#define GR_FILTERMASK_ALL 7
#define GR_MAX_TEXTURE_MIPMAPS 11
#define GR_MAX_TEXTURE_DIMENSION_LOG2 10
#define GR_MAX_TEXTURE_UNITS 2

enum GR_TEXCOORD_e
{
    GR_TEXCOORD_S = 0,
    GR_TEXCOORD_T = 1,
    GR_TEXCOORD_MAX = 2
};

/* one mip level (decompile cv_anon_1472; grInitTexture writes exactly these) */
typedef struct _grMipLevel
{
    void *pixels;
    int log2Dim[2];
    int texFormat;
    int initialized;
    int pbuffer;
} _grMipLevel;

/* texture object (decompile cv_anon_1477). Field ORDER in memory is not
 * pinned by the decompile (fields are always accessed by name); only the
 * field set is. */
typedef struct _grTexture
{
    unsigned int parmMask;
    unsigned char filterMask[2];
    unsigned char wrapMode[2];
    unsigned char target;
    unsigned char numMipLevels;
    void *palette;
    _grMipLevel mipmap[GR_MAX_TEXTURE_MIPMAPS];
    _grRect cropRect;
    int genMipMaps;
} _grTexture;

int grIsValidTexture(_grTexture *texture);
void grUpdateTexture(_grTexture *texture);
int grGetTextureTargetDimensionality(int texTarget);

void grInitTexture(_grTexture *texture);
int  grGetTextureTarget(_grTexture *texture);
void grSetTextureTarget(_grTexture *texture, int texTarget);
void *grGetTextureMipLevel(_grTexture *texture, int level, int *log2Dim);
int  grGetTextureMipLevelFormat(_grTexture *texture, int level);
int  grTextureMipLevelInitialized(_grTexture *texture, int level);
void grSetTextureWrapMode(_grTexture *texture, int coord, int wrapMode);
unsigned char grGetTextureWrapMode(_grTexture *texture, int coord);
void grSetTextureGenMipmaps(_grTexture *texture, int genMipmaps);
int  grGetTextureGenMipmaps(_grTexture *texture);
void grSetTextureCropRect(_grTexture *texture, _grRect *rect);
void grGetTextureCropRect(_grTexture *texture, _grRect *rect);
void grSetTextureFilter(_grTexture *texture, int filterType, unsigned int filterMask);
unsigned int grGetTextureFilter(_grTexture *texture, int filterType);

int grIsPalettedTextureFormat(int texFormat);

HG_EXTERN_C_END

#define GR_TEXTURE_CHECK(cond, file, line)       \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, (file), (line)) != 0) \
            grDie(0);                            \
    } while (0)

#endif /* GR_TEXTURE_H */
