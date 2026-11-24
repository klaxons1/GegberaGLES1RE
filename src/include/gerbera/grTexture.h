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
 *    15..17 are RGB565, RGBA4444, and RGBA5551; grInitTexture seeds
 *    texFormat = 18 = MAX.
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
    GR_TEXTUREFORMAT_ALPHA8 = 0,
    GR_TEXTUREFORMAT_RGB8 = 1,
    GR_TEXTUREFORMAT_RGBA8 = 2,
    GR_TEXTUREFORMAT_LUMINANCE8 = 3,
    GR_TEXTUREFORMAT_LUMINANCE8_ALPHA8 = 4,
    GR_TEXTUREFORMAT_PALETTE4_RGB8 = 5,
    GR_TEXTUREFORMAT_PALETTE4_RGBA8 = 6,
    GR_TEXTUREFORMAT_PALETTE4_RGB565 = 7,
    GR_TEXTUREFORMAT_PALETTE4_RGBA4 = 8,
    GR_TEXTUREFORMAT_PALETTE4_RGBA5551 = 9,
    GR_TEXTUREFORMAT_PALETTE8_RGB8 = 10,
    GR_TEXTUREFORMAT_PALETTE8_RGBA8 = 11,
    GR_TEXTUREFORMAT_PALETTE8_RGB565 = 12,
    GR_TEXTUREFORMAT_PALETTE8_RGBA4 = 13,
    GR_TEXTUREFORMAT_PALETTE8_RGBA5551 = 14,
    GR_TEXTUREFORMAT_RGB565 = 15,
    GR_TEXTUREFORMAT_RGBA4444 = 16,
    GR_TEXTUREFORMAT_RGBA5551 = 17,
    GR_TEXTUREFORMAT_MAX = 18,
    /* Alternate format names. */
    GR_TEXTUREFORMAT_ALPHA = 0,
    GR_TEXTUREFORMAT_RGB = 1,
    GR_TEXTUREFORMAT_RGBA = 2,
    GR_TEXTUREFORMAT_LUMINANCE = 3,
    GR_TEXTUREFORMAT_LUMINANCE_ALPHA = 4,
    GR_TEXTUREFORMAT_PALETTED_FIRST = 5,
    GR_TEXTUREFORMAT_PALETTED_LAST = 14,
    GR_TEXTUREFORMAT_15 = 15,
    GR_TEXTUREFORMAT_16 = 16,
    GR_TEXTUREFORMAT_17 = 17
};

enum GR_WRAPMODE_e
{
    GR_WRAPMODE_REPEAT = 0,
    GR_WRAPMODE_CLAMP_TO_EDGE = 1,
    GR_WRAPMODE_MIRRORED_REPEAT = 2,
    GR_WRAPMODE_0 = 0,
    GR_WRAPMODE_1 = 1,
    GR_WRAPMODE_2 = 2,
    GR_WRAPMODE_MAX = 3
};

enum GR_FILTERTYPE_e
{
    GR_FILTERTYPE_MIN = 0,
    GR_FILTERTYPE_MAG = 1,
    GR_FILTERTYPE_MAX = 2
};

#define GR_FILTERMASK_FILTER 1
#define GR_FILTERMASK_USE_MIPMAPS 2
#define GR_FILTERMASK_FILTER_MIPMAPS 4
#define GR_FILTERMASK_ALL 7
#define GR_MAX_TEXTURE_MIPMAPS 11
#define GR_MAX_TEXTURE_DIMENSION 1024
#define GR_MAX_TEXTURE_DIMENSION_LOG2 10
#define GR_MAX_TEXTURE_UNITS 2

enum GR_TEXCOORD_e
{
    GR_TEXCOORD_S = 0,
    GR_TEXCOORD_T = 1,
    GR_TEXCOORD_MAX = 2
};

enum GR_TEXENVMODE_e
{
    GR_TEXENVMODE_MODULATE = 0,
    GR_TEXENVMODE_DECAL = 1,
    GR_TEXENVMODE_BLEND = 2,
    GR_TEXENVMODE_REPLACE = 3,
    GR_TEXENVMODE_ADD = 4,
    GR_TEXENVMODE_COMBINE = 5,
    GR_TEXENVMODE_MAX = 6
};

enum GR_TEXENVCOMBFUNC_e
{
    GR_TEXENVCOMBFUNC_MODULATE = 0,
    GR_TEXENVCOMBFUNC_REPLACE = 1,
    GR_TEXENVCOMBFUNC_ADD = 2,
    GR_TEXENVCOMBFUNC_ADD_SIGNED = 3,
    GR_TEXENVCOMBFUNC_INTERPOLATE = 4,
    GR_TEXENVCOMBFUNC_SUBTRACT = 5,
    GR_TEXENVCOMBFUNC_DOT3_RGB = 6,
    GR_TEXENVCOMBFUNC_DOT3_RGBA = 7,
    GR_TEXENVCOMBFUNC_MAX = 8
};

enum GR_TEXENVCOMBSRC_e
{
    GR_TEXENVCOMBSRC_TEXTURE = 0,
    GR_TEXENVCOMBSRC_CONSTANT = 1,
    GR_TEXENVCOMBSRC_PRIMARY_COLOR = 2,
    GR_TEXENVCOMBSRC_PREVIOUS = 3,
    GR_TEXENVCOMBSRC_MAX = 4
};

enum GR_TEXENVCOMBOPERAND_e
{
    GR_TEXENVCOMBOPERAND_SRC_COLOR = 0,
    GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_COLOR = 1,
    GR_TEXENVCOMBOPERAND_SRC_ALPHA = 2,
    GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_ALPHA = 3,
    GR_TEXENVCOMBOPERAND_MAX = 4
};

struct Gerbera_s;

/* one mip level (CodeView TI=0x1472, 16 bytes on 32-bit) */
typedef struct _grMipLevel
{
    void *pixels;
    short log2Dim[2];
    unsigned short texFormat;
    unsigned short initialized;
    int pbuffer;
} _grMipLevel;

/* texture format info row (CodeView TI=0x182B; s_infos[18] in
 * data.fragments.c 3351, indexed by GR_TEXTUREFORMAT) */
typedef struct _grTextureFormatInfo
{
    unsigned char colorBytes;
    unsigned char rSize;
    unsigned char gSize;
    unsigned char bSize;
    unsigned char aSize;
    unsigned char rOffset;
    unsigned char gOffset;
    unsigned char bOffset;
    unsigned char aOffset;
    unsigned char pixelFormat;
    unsigned char paletteFormat;
    unsigned char padding;
} _grTextureFormatInfo;             /* 12 bytes */

/* texture object (CodeView TI=0x1477, 208 bytes on 32-bit):
 *   +0   mipmap[11]  (11 * 16 = 176 bytes; mipmap[0].texFormat at +8)
 *   +176 palette     (void*)
 *   +180 numMipLevels (short)
 *   +182 target      (unsigned char)
 *   +183 parmMask    (unsigned char: 1=dirty, 2=checked, 4=complete, 8=chain,
 *                     0x10=square, 0x20=min!=mag)
 *   +184 genMipMaps  (int)
 *   +188 cropRect    (_grRect, 16 bytes)
 *   +204 wrapMode[2] (unsigned char[2])
 *   +206 filterMask[2] (unsigned char[2]: MIN=0, MAG=1) */
typedef struct _grTexture
{
    _grMipLevel mipmap[GR_MAX_TEXTURE_MIPMAPS];
    void *palette;
    short numMipLevels;
    unsigned char target;
    unsigned char parmMask;
    int genMipMaps;
    _grRect cropRect;
    unsigned char wrapMode[2];
    unsigned char filterMask[2];
} _grTexture;

int grIsValidTexture(_grTexture *texture);
void grUpdateTexture(_grTexture *texture);
int  grrGetBitsPerPixel(int texFormat);
void grrGetTextureFormatDescriptor(_grSurfaceDescriptor *desc, int texFormat);
void grrSelfTest(void);
int  grrSetupTextureLevel(_grTexture *texture, int mipLevel, int width, int height, int texFormat);
void grrReplaceTextureLevel(_grTexture *texture, int mipLevel, void *pixels, int width, int height, int texFormat);
int  grrCalcTextureMipmaps(struct Gerbera_s *state, _grTexture *texture);
void grrFreeTextureLevel(_grTexture *texture, int mipLevel);
void grrFreeTextureData(_grTexture *texture);
int  grrSetPalettedTexture(_grTexture *texture, _grSurfaceDescriptor *inDesc, int texFormat,
                           int width, int height, int numLevels, void *data);

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
