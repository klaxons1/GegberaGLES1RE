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
    /* Legacy aliases preserved for existing tests */
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

/* texture format info row (decompile cv_anon_182B; s_infos[18] in
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

/* texture object (decompile cv_anon_1477). The named fields are always
 * accessed by name; the RAW-OFFSET consumers (grEvaluatePixelPipeline,
 * grRasterizer texture setup) pin these byte positions:
 *   +8   u16 texFormat (the descriptor source)
 *   +176 palette base pointer (copied into the pipe unit +8)
 *   +183 flags byte (0x10 -> signature, 0x20 -> rasterizer enable)
 *   +204/205 per-coord texcoord data format (0=u16, 1=u8, 2=u10; asserted)
 *   +206 flags (bit0 -> signature, bit1 -> rasterizer)
 *   +207 flags (bit0 -> signature)
 * The mipmap base offset is NOT pinned; everything between numMipLevels
 * and palette, and between palette and the coord formats, is unnamed DLL
 * data (pads below reserve the pinned gaps). */
typedef struct _grTexture
{
    unsigned int parmMask;              /* +0 */
    unsigned char filterMask[2];        /* +4 */
    unsigned char wrapMode[2];          /* +6 */
    unsigned short texFormat;           /* +8  (DLL u16, pinned) */
    unsigned char target;               /* +10 */
    unsigned char numMipLevels;         /* +11 */
    unsigned char pad12[164];           /* +12..175 (DLL unnamed params) */
    void *palette;                      /* +176 (DLL, pinned) */
    unsigned char pad180[3];            /* +180..182 */
    unsigned char flags183;             /* +183 (DLL, pinned) */
    unsigned char pad184[20];           /* +184..203 (DLL unnamed) */
    unsigned char coordFormat[2];       /* +204/205 (DLL, pinned) */
    unsigned char flags206;             /* +206 (DLL, pinned) */
    unsigned char flags207;             /* +207 (DLL, pinned) */
    _grMipLevel mipmap[GR_MAX_TEXTURE_MIPMAPS]; /* base UNPINNED */
    _grRect cropRect;
    int genMipMaps;
} _grTexture;

int grIsValidTexture(_grTexture *texture);
void grUpdateTexture(_grTexture *texture);
void grrGetTextureFormatDescriptor(_grSurfaceDescriptor *desc, int texFormat);

/* texture-env combine ranges (grEvaluatePixelPipeline asserts) */
#define GR_TEXENVCOMBSRC_MAX 4
#define GR_TEXENVCOMBFUNC_MAX 8
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
void grrFreeTextureLevel(_grTexture *texture, int mipLevel);
void grrFreeTextureData(_grTexture *texture);

HG_EXTERN_C_END

#define GR_TEXTURE_CHECK(cond, file, line)       \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, (file), (line)) != 0) \
            grDie(0);                            \
    } while (0)

#endif /* GR_TEXTURE_H */
