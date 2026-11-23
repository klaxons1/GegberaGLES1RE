/* oglUtil.c - OpenGL ES 1.1 input/color converters, GL-token -> Gerbera-enum
 * maps and unmaps, self-test, memory-image/texture-unit helpers, and shared
 * material/lighting/client-state workers (oglUtil.inl).
 *
 * Assert sites keep the original oglUtil.inl file/line strings.
 */
#include "gerbera/api_opengl/oglContext.h"
#include "gerbera/grRGBA.h"
#include "hg/hgInt32.h"
#include "hg/hgFloat.h"
#include "imath/imApi.h"

typedef struct _oglEnumPair
{
    unsigned short src;
    unsigned short dst;
} _oglEnumPair;

/* --- .rdata lookup tables -------------------------------------------------- */

static const unsigned short s_capKeys[23] =
{
    0x0B10, 0x0B20, 0x0B44, 0x0B50, 0x0B57, 0x0B60, 0x0B71, 0x0B90,
    0x0BA1, 0x0BC0, 0x0BD0, 0x0BE2, 0x0BF2, 0x0C11, 0x0DE1, 0x8037,
    0x803A, 0x809D, 0x809E, 0x809F, 0x80A0, 0x8840, 0x8861
};

static const unsigned char s_capValues[23] =
{
    19, 18, 16, 36, 40,  7,  2,  3,
    37,  1,  6,  4,  5, 15, 10, 14,
    38, 49, 50, 51, 52, 53, 20
};

static const _oglEnumPair s_imageFormats[5] =
{
    { 0x1906, GR_IMAGEFORMAT_ALPHA },
    { 0x1907, GR_IMAGEFORMAT_RGB },
    { 0x1908, GR_IMAGEFORMAT_RGBA },
    { 0x1909, GR_IMAGEFORMAT_LUMINANCE },
    { 0x190A, GR_IMAGEFORMAT_LUMINANCE_ALPHA }
};

static const _oglEnumPair s_stencilOps[6] =
{
    { 0x1E00, GR_STENCILOP_KEEP },
    { 0x1E01, GR_STENCILOP_REPLACE },
    { 0x1E02, GR_STENCILOP_INCR },
    { 0x1E03, GR_STENCILOP_DECR },
    { 0x0000, GR_STENCILOP_ZERO },
    { 0x150A, GR_STENCILOP_INVERT }
};

static const _oglEnumPair s_materialColors[4] =
{
    { 0x1200, GR_MATERIALCOLOR_AMBIENT },
    { 0x1201, GR_MATERIALCOLOR_DIFFUSE },
    { 0x1202, GR_MATERIALCOLOR_SPECULAR },
    { 0x1600, GR_MATERIALCOLOR_EMISSION }
};

static const _oglEnumPair s_logicOps[16] =
{
    { 0x1500, GR_LOGICOP_CLEAR },
    { 0x1501, GR_LOGICOP_AND },
    { 0x1502, GR_LOGICOP_AND_REVERSE },
    { 0x1503, GR_LOGICOP_COPY },
    { 0x1504, GR_LOGICOP_AND_INVERTED },
    { 0x1505, GR_LOGICOP_NOOP },
    { 0x1506, GR_LOGICOP_XOR },
    { 0x1507, GR_LOGICOP_OR },
    { 0x1508, GR_LOGICOP_NOR },
    { 0x1509, GR_LOGICOP_EQUIV },
    { 0x150A, GR_LOGICOP_INVERT },
    { 0x150B, GR_LOGICOP_OR_REVERSE },
    { 0x150C, GR_LOGICOP_COPY_INVERTED },
    { 0x150D, GR_LOGICOP_OR_INVERTED },
    { 0x150E, GR_LOGICOP_NAND },
    { 0x150F, GR_LOGICOP_SET }
};

static const _oglEnumPair s_texEnvModes[5] =
{
    { 0x2100, GR_TEXENVMODE_MODULATE },
    { 0x2101, GR_TEXENVMODE_DECAL },
    { 0x0BE2, GR_TEXENVMODE_BLEND },
    { 0x1E01, GR_TEXENVMODE_REPLACE },
    { 0x0104, GR_TEXENVMODE_ADD }
};

static const _oglEnumPair s_internalTextureFormats[5] =
{
    { 0x1906, GR_TEXTUREFORMAT_ALPHA8 },
    { 0x1907, GR_TEXTUREFORMAT_RGB8 },
    { 0x1908, GR_TEXTUREFORMAT_RGBA8 },
    { 0x1909, GR_TEXTUREFORMAT_LUMINANCE8 },
    { 0x190A, GR_TEXTUREFORMAT_LUMINANCE8_ALPHA8 }
};

/* --- Data-type & scalar/color conversions ---------------------------------- */

int oglMapDataType(unsigned int type)
{
    switch (type)
    {
    case GL_BYTE:
        return GR_DATATYPE_INT8;
    case GL_UNSIGNED_BYTE:
        return GR_DATATYPE_UINT8;
    case GL_SHORT:
        return GR_DATATYPE_INT16;
    case GL_UNSIGNED_SHORT:
        return GR_DATATYPE_UINT16;
    case GL_FLOAT:
        return GR_DATATYPE_FLOAT;
    case GL_FIXED:
        return GR_DATATYPE_FIXED;
    default:
        return GR_DATATYPE_MAX;
    }
}

float oglFixedToFloat(int v)
{
    return IScalar_stof(IScalar_fixedToScalar(v, 16));
}

float oglFloatClampUnit(float f)
{
    return IScalar_stof(IScalar_clampUnit(IScalar_ftos(f)));
}

float oglFixedClampUnit(int x)
{
    return IScalar_stof(IScalar_fixedToScalar(hgClamp32_0_x(x, 0x10000), 16));
}

int oglFloatToFixed(float v)
{
    return IScalar_scalarToFixed(IScalar_ftos(v), 16);
}

unsigned int oglFloatsToRGBA(float r, float g, float b, float a)
{
    int components[4];
    unsigned int color = 0;
    int i;

    components[0] = (int)hgFloatBitPattern(r);
    components[1] = (int)hgFloatBitPattern(g);
    components[2] = (int)hgFloatBitPattern(b);
    components[3] = (int)hgFloatBitPattern(a);
    for (i = GR_COLOR_A; i >= GR_COLOR_R; --i)
    {
        unsigned char c = (unsigned char)hgClamp32_0_x(
            IScalar_toFixedHO(IScalar_iftos(components[i]), 8), 255);
        color |= grPackRGBA(c, i);
    }
    return color;
}

unsigned int oglFixedToRGBA(int r, int g, int b, int a)
{
    int ca = hgClamp32_0_x((255 * (a >> 8)) >> 8, 255);
    int cb = hgClamp32_0_x((255 * (b >> 8)) >> 8, 255);
    int cg = hgClamp32_0_x((255 * (g >> 8)) >> 8, 255);
    int cr = hgClamp32_0_x((255 * (r >> 8)) >> 8, 255);
    return grSetRGBA(cr, cg, cb, ca);
}

void oglInputToXColor3(_XColor3 *color, const void *params, OGL_INPUTTYPE inputType)
{
    const int *p;

    OGL_ASSERT_AT("oglUtil.inl", 155, "params", params != NULL);
    p = (const int *)params;
    if (inputType != OGL_INPUTTYPE_FLOAT)
        XColor3_setx(color, p[0], p[1], p[2]);
    else
        XColor3_setf(color, (unsigned int)p[0], (unsigned int)p[1], (unsigned int)p[2]);
}

unsigned short oglInputAlpha(const void *params, OGL_INPUTTYPE inputType)
{
    int a;

    OGL_ASSERT_AT("oglUtil.inl", 176, "params", params != NULL);
    a = ((const int *)params)[3];
    if (inputType != OGL_INPUTTYPE_FLOAT)
    {
        if (inputType != OGL_INPUTTYPE_FIXED)
            imAssert("inputType == OGL_INPUTTYPE_FIXED", "oglUtil.inl", 183);
        return (unsigned short)hgClamp32_0_x(a, 0xFFFF);
    }
    return (unsigned short)hgClamp32_0_x(IScalar_scalarToFixed(IScalar_iftos(a), 16), 0xFFFF);
}

unsigned int oglInputToRGBA(const void *params, OGL_INPUTTYPE inputType)
{
    const int *p = (const int *)params;
    int r = p[0];
    int g = p[1];
    int b = p[2];
    int a = p[3];
    int ra, ga, ba, aa;

    switch (inputType)
    {
    case OGL_INPUTTYPE_FIXED:
        ra = r >> 8;
        ga = g >> 8;
        ba = b >> 8;
        aa = a >> 8;
        break;
    case OGL_INPUTTYPE_INT:
        ra = r >> 23;
        ga = g >> 23;
        ba = b >> 23;
        aa = a >> 23;
        break;
    default:
        if (inputType != OGL_INPUTTYPE_FLOAT)
            imAssert("inputType == OGL_INPUTTYPE_FLOAT", "oglUtil.inl", 222);
        ra = IScalar_scalarToFixed(IScalar_iftos(r), 8);
        ga = IScalar_scalarToFixed(IScalar_iftos(g), 8);
        ba = IScalar_scalarToFixed(IScalar_iftos(b), 8);
        aa = IScalar_scalarToFixed(IScalar_iftos(a), 8);
        break;
    }
    return (unsigned int)hgClamp32_0_x(ra, 255) |
           ((unsigned int)hgClamp32_0_x(ga, 255) << 8) |
           ((unsigned int)hgClamp32_0_x(ba, 255) << 16) |
           ((unsigned int)hgClamp32_0_x(aa, 255) << 24);
}

int oglInputToIScalarDirect(const void *param, OGL_INPUTTYPE inputType)
{
    OGL_ASSERT_AT("oglUtil.inl", 246,
                  "inputType==OGL_INPUTTYPE_FLOAT || inputType==OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED);
    if (inputType == OGL_INPUTTYPE_FLOAT)
        return IScalar_iftos(*(const int *)param);
    OGL_ASSERT_AT("oglUtil.inl", 251, "inputType == OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FIXED);
    return IScalar_fixedToScalar(*(const int *)param, 16);
}

int oglTestInput(const void *param, int ndx, OGL_INPUTTYPE inputType)
{
    int p;

    OGL_ASSERT_AT("oglUtil.inl", 265, "param", param != NULL);
    OGL_ASSERT_AT("oglUtil.inl", 266,
                  "inputType==OGL_INPUTTYPE_FLOAT || inputType==OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED);
    p = ((const int *)param)[ndx];
    if (inputType == OGL_INPUTTYPE_FLOAT && (unsigned int)p == 0x80000000u)
        p = 0;
    return hgSign32(p);
}

float oglInputToFloatDirect(const void *param, OGL_INPUTTYPE inputType)
{
    OGL_ASSERT_AT("oglUtil.inl", 291,
                  "inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED);
    if (inputType != OGL_INPUTTYPE_FLOAT)
    {
        OGL_ASSERT_AT("oglUtil.inl", 296, "inputType == OGL_INPUTTYPE_FIXED",
                      inputType == OGL_INPUTTYPE_FIXED);
        return oglFixedToFloat(*(const int *)param);
    }
    return *(const float *)param;
}

void oglInputToIFVector4Direct(_IFVector4 *dst, const void *params, OGL_INPUTTYPE inputType)
{
    OGL_ASSERT_AT("oglUtil.inl", 313,
                  "inputType==OGL_INPUTTYPE_FLOAT || inputType==OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED);
    if (inputType != OGL_INPUTTYPE_FLOAT)
    {
        OGL_ASSERT_AT("oglUtil.inl", 321, "inputType == OGL_INPUTTYPE_FIXED",
                      inputType == OGL_INPUTTYPE_FIXED);
        IFVector4_setxv(dst, (int *)params, 4);
    }
    else
    {
        IFVector4_setfv(dst, (int *)params, 4);
    }
}

void oglInputToIFVector4W0(_IFVector4 *dst, const void *params, OGL_INPUTTYPE inputType)
{
    int p[4];

    OGL_ASSERT_AT("oglUtil.inl", 340,
                  "inputType==OGL_INPUTTYPE_FLOAT || inputType==OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED);
    p[0] = ((const int *)params)[0];
    p[1] = ((const int *)params)[1];
    p[2] = ((const int *)params)[2];
    p[3] = 0;
    if (inputType != OGL_INPUTTYPE_FLOAT)
        IFVector4_setxv(dst, p, 4);
    else
        IFVector4_setfv(dst, p, 4);
}

int oglInputToInteger(const void *param, OGL_INPUTTYPE inputType)
{
    if (inputType == OGL_INPUTTYPE_INT || inputType == OGL_INPUTTYPE_FIXED)
        return *(const int *)param;
    return IScalar_scalarToFixed(oglInputToIScalarDirect(param, inputType), 0);
}

unsigned int oglInputToEnum(const void *param, OGL_INPUTTYPE inputType)
{
    return (unsigned int)oglInputToInteger(param, inputType);
}

void oglInputToFloatv(float *fArr, const void *param, OGL_INPUTTYPE inputType, int n)
{
    int i;

    OGL_ASSERT_AT("oglUtil.inl", 400, "fArr && param", fArr != NULL && param != NULL);
    OGL_ASSERT_AT("oglUtil.inl", 401,
                  "inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED",
                  inputType == OGL_INPUTTYPE_FLOAT || inputType == OGL_INPUTTYPE_FIXED);
    OGL_ASSERT_AT("oglUtil.inl", 402, "n > 0", n > 0);
    for (i = n - 1; i >= 0; --i)
        fArr[i] = oglInputToFloatDirect((const unsigned char *)param + 4 * i, inputType);
}

void oglRGBAToFloatv(unsigned int c, float *v)
{
    int i;

    OGL_ASSERT_AT("oglUtil.inl", 423, "v", v != NULL);
    for (i = GR_COLOR_A; i >= GR_COLOR_R; --i)
    {
        unsigned char byte = grUnpackRGBA(c, i);
        v[i] = IScalar_stof(IScalar_byteToScalar(byte));
    }
}

/* --- Binary search & GL-token <-> Gerbera-enum mappers --------------------- */

int oglBinarySearch(int key, int length, const unsigned short *keys)
{
    int l = 0;
    int r = length - 1;
    int i;

    for (i = 1; i < length; ++i)
        OGL_ASSERT_AT("oglUtil.inl", 454, "keys[i-1] < keys[i]", keys[i - 1] < keys[i]);

    do
    {
        int x = (r + l) >> 1;
        if (key >= (int)keys[x])
            l = x + 1;
        else
            r = x - 1;
        if (key == (int)keys[x])
            return x;
    } while (l <= r);

    return -1;
}

int oglMapFogMode(unsigned int mode)
{
    switch (mode)
    {
    case GL_EXP:
        return GR_FOGMODE_EXP;
    case GL_EXP2:
        return GR_FOGMODE_EXP2;
    case GL_LINEAR:
        return GR_FOGMODE_LINEAR;
    default:
        return GR_FOGMODE_MAX;
    }
}

int oglMapMatrixMode(unsigned int mode)
{
    switch (mode)
    {
    case GL_MODELVIEW:
        return GR_MATRIXMODE_MODELVIEW;
    case GL_PROJECTION:
        return GR_MATRIXMODE_PROJECTION;
    case GL_TEXTURE:
        return GR_MATRIXMODE_TEXTURE0;
    case GL_MATRIX_PALETTE_OES:
        return GR_MATRIXMODE_PALETTE0;
    default:
        return GR_MATRIXMODE_MAX;
    }
}

OGL_STRINGNAME oglMapStringName(unsigned int name)
{
    if (hgInRange32((int)(name - GL_VENDOR), 0, 3) != 0)
        return (OGL_STRINGNAME)(name - GL_VENDOR);
    return OGL_STRINGNAME_MAX;
}

int oglMapImageFormat(unsigned int format)
{
    switch (format)
    {
    case GL_ALPHA:
        return GR_IMAGEFORMAT_ALPHA;
    case GL_RGB:
        return GR_IMAGEFORMAT_RGB;
    case GL_RGBA:
        return GR_IMAGEFORMAT_RGBA;
    case GL_LUMINANCE:
        return GR_IMAGEFORMAT_LUMINANCE;
    case GL_LUMINANCE_ALPHA:
        return GR_IMAGEFORMAT_LUMINANCE_ALPHA;
    default:
        return GR_IMAGEFORMAT_MAX;
    }
}

int oglMapCapability(unsigned int target)
{
    int ndx;

    if (hgInRange32((int)target, GL_LIGHT0, GL_LIGHT7) != 0)
        return (int)(target - (GL_LIGHT0 - GR_CAPABILITY_LIGHT0));
    if (hgInRange32((int)target, GL_CLIP_PLANE0, GL_CLIP_PLANE5) != 0)
        return (int)(target - (GL_CLIP_PLANE0 - GR_CAPABILITY_CLIP_PLANE0));
    ndx = oglBinarySearch((int)target, 23, s_capKeys);
    if (ndx == -1)
        return GR_CAPABILITY_MAX;
    OGL_ASSERT_AT("oglUtil.inl", 647, "hgInRange32(ndx, 0, HG_LENGTH_OF_ARRAY(values))",
                  hgInRange32(ndx, 0, 23) != 0);
    return s_capValues[ndx];
}

int oglMapStencilOp(unsigned int op)
{
    if (hgInRange32((int)op, GL_KEEP, GL_DECR) != 0)
        return (int)(op - GL_KEEP);
    if (op == GL_ZERO)
        return GR_STENCILOP_ZERO;
    if (op == GL_INVERT)
        return GR_STENCILOP_INVERT;
    return GR_STENCILOP_MAX;
}

int oglMapBlendFactor(unsigned int factor)
{
    if (factor <= GL_ONE)
        return (int)factor;
    if (hgInRange32((int)factor, GL_SRC_COLOR, GL_SRC_ALPHA_SATURATE) != 0)
        return (int)(factor - (GL_SRC_COLOR - GR_BLENDFACTOR_SRC_COLOR));
    return GR_BLENDFACTOR_MAX;
}

int oglMapLogicOp(unsigned int op)
{
    if (hgInRange32((int)op, GL_CLEAR, GL_SET) != 0)
        return (int)(op - GL_CLEAR);
    return GR_LOGICOP_MAX;
}

int oglMapClientStateCapability(oglContext_s *context, unsigned int apiCap)
{
    int cap = (int)(apiCap - 32863u);

    if (cap >= 25)
    {
        OGL_ASSERT_AT("oglUtil.inl", 716,
                      "hgInRange32(context->clientActiveTexture, 0, GR_MAX_TEXTURE_UNITS-1)",
                      hgInRange32(context->clientActiveTexture, 0, GR_MAX_TEXTURE_UNITS - 1) != 0);
        if (apiCap == GL_TEXTURE_COORD_ARRAY)
            cap = context->clientActiveTexture + GR_CAPABILITY_TEXTURE_COORD_ARRAY_0;
        else
            cap = GR_CAPABILITY_ALPHA_TEST;
    }
    if (hgInRange32(cap, GR_CAPABILITY_VERTEX_ARRAY, GR_CAPABILITY_TEXTURE_COORD_ARRAY_1) != 0 &&
        cap != GR_CAPABILITY_INDEX_ARRAY)
    {
        return cap;
    }
    switch (apiCap)
    {
    case GL_WEIGHT_ARRAY_OES:
        return GR_CAPABILITY_WEIGHT_ARRAY;
    case GL_MATRIX_INDEX_ARRAY_OES:
        return GR_CAPABILITY_MATRIX_INDEX_ARRAY;
    case GL_POINT_SIZE_ARRAY_OES:
        return GR_CAPABILITY_POINT_SIZE_ARRAY;
    default:
        return GR_CAPABILITY_MAX;
    }
}

int oglMapMaterialColor(unsigned int param)
{
    if (hgInRange32((int)param, GL_AMBIENT, GL_SPECULAR) != 0)
        return (int)(param - GL_AMBIENT);
    if (param == GL_EMISSION)
        return GR_MATERIALCOLOR_EMISSION;
    return GR_MATERIALCOLOR_MAX;
}

int oglMapTexEnvMode(unsigned int pname)
{
    switch (pname)
    {
    case GL_ADD:
        return GR_TEXENVMODE_ADD;
    case GL_BLEND:
        return GR_TEXENVMODE_BLEND;
    case GL_REPLACE:
        return GR_TEXENVMODE_REPLACE;
    case GL_MODULATE:
        return GR_TEXENVMODE_MODULATE;
    case GL_DECAL:
        return GR_TEXENVMODE_DECAL;
    case GL_COMBINE:
        return GR_TEXENVMODE_COMBINE;
    default:
        return GR_TEXENVMODE_MAX;
    }
}

unsigned int oglUnmapTexEnvMode(int pname)
{
    switch (pname)
    {
    case GR_TEXENVMODE_MODULATE:
        return GL_MODULATE;
    case GR_TEXENVMODE_DECAL:
        return GL_DECAL;
    case GR_TEXENVMODE_BLEND:
        return GL_BLEND;
    case GR_TEXENVMODE_REPLACE:
        return GL_REPLACE;
    case GR_TEXENVMODE_COMBINE:
        return GL_COMBINE;
    default:
        OGL_ASSERT_AT("oglUtil.inl", 803, "pname == GR_TEXENVMODE_ADD",
                      pname == GR_TEXENVMODE_ADD);
        return GL_ADD;
    }
}

int oglMapTexEnvCombFunc(unsigned int pname)
{
    switch (pname)
    {
    case GL_ADD:
        return GR_TEXENVCOMBFUNC_ADD;
    case GL_REPLACE:
        return GR_TEXENVCOMBFUNC_REPLACE;
    case GL_MODULATE:
        return GR_TEXENVCOMBFUNC_MODULATE;
    case GL_SUBTRACT:
        return GR_TEXENVCOMBFUNC_SUBTRACT;
    case GL_ADD_SIGNED:
        return GR_TEXENVCOMBFUNC_ADD_SIGNED;
    case GL_INTERPOLATE:
        return GR_TEXENVCOMBFUNC_INTERPOLATE;
    case GL_DOT3_RGB:
        return GR_TEXENVCOMBFUNC_DOT3_RGB;
    case GL_DOT3_RGBA:
        return GR_TEXENVCOMBFUNC_DOT3_RGBA;
    default:
        return GR_TEXENVCOMBFUNC_MAX;
    }
}

unsigned int oglUnmapTexEnvCombFunc(int pname)
{
    switch (pname)
    {
    case GR_TEXENVCOMBFUNC_MODULATE:
        return GL_MODULATE;
    case GR_TEXENVCOMBFUNC_REPLACE:
        return GL_REPLACE;
    case GR_TEXENVCOMBFUNC_ADD:
        return GL_ADD;
    case GR_TEXENVCOMBFUNC_ADD_SIGNED:
        return GL_ADD_SIGNED;
    case GR_TEXENVCOMBFUNC_INTERPOLATE:
        return GL_INTERPOLATE;
    case GR_TEXENVCOMBFUNC_SUBTRACT:
        return GL_SUBTRACT;
    case GR_TEXENVCOMBFUNC_DOT3_RGB:
        return GL_DOT3_RGB;
    default:
        OGL_ASSERT_AT("oglUtil.inl", 854, "pname == GR_TEXENVCOMBFUNC_DOT3_RGBA",
                      pname == GR_TEXENVCOMBFUNC_DOT3_RGBA);
        return GL_DOT3_RGBA;
    }
}

int oglMapTexEnvCombSrc(unsigned int pname)
{
    switch (pname)
    {
    case GL_TEXTURE:
        return GR_TEXENVCOMBSRC_TEXTURE;
    case GL_CONSTANT:
        return GR_TEXENVCOMBSRC_CONSTANT;
    case GL_PRIMARY_COLOR:
        return GR_TEXENVCOMBSRC_PRIMARY_COLOR;
    case GL_PREVIOUS:
        return GR_TEXENVCOMBSRC_PREVIOUS;
    default:
        return GR_TEXENVCOMBSRC_MAX;
    }
}

unsigned int oglUnmapTexEnvCombSrc(int pname)
{
    switch (pname)
    {
    case GR_TEXENVCOMBSRC_TEXTURE:
        return GL_TEXTURE;
    case GR_TEXENVCOMBSRC_CONSTANT:
        return GL_CONSTANT;
    case GR_TEXENVCOMBSRC_PRIMARY_COLOR:
        return GL_PRIMARY_COLOR;
    default:
        OGL_ASSERT_AT("oglUtil.inl", 897, "pname == GR_TEXENVCOMBSRC_PREVIOUS",
                      pname == GR_TEXENVCOMBSRC_PREVIOUS);
        return GL_PREVIOUS;
    }
}

int oglMapTexEnvCombOperand(unsigned int pname)
{
    switch (pname)
    {
    case GL_SRC_COLOR:
        return GR_TEXENVCOMBOPERAND_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR:
        return GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_COLOR;
    case GL_SRC_ALPHA:
        return GR_TEXENVCOMBOPERAND_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_ALPHA;
    default:
        return GR_TEXENVCOMBOPERAND_MAX;
    }
}

unsigned int oglUnmapTexEnvCombOperand(int pname)
{
    switch (pname)
    {
    case GR_TEXENVCOMBOPERAND_SRC_COLOR:
        return GL_SRC_COLOR;
    case GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_COLOR:
        return GL_ONE_MINUS_SRC_COLOR;
    case GR_TEXENVCOMBOPERAND_SRC_ALPHA:
        return GL_SRC_ALPHA;
    default:
        OGL_ASSERT_AT("oglUtil.inl", 940,
                      "pname == GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_ALPHA",
                      pname == GR_TEXENVCOMBOPERAND_ONE_MINUS_SRC_ALPHA);
        return GL_ONE_MINUS_SRC_ALPHA;
    }
}

int oglMapInternalTextureFormat(unsigned int internalFormat)
{
    if (hgInRange32((int)internalFormat, GL_ALPHA, GL_LUMINANCE_ALPHA) != 0)
        return (int)(internalFormat - GL_ALPHA);
    return GR_TEXTUREFORMAT_MAX;
}

int oglIsPalettedTextureFormat(int texFormat)
{
    return hgInRange32(texFormat, GR_TEXTUREFORMAT_PALETTE4_RGB8, GR_TEXTUREFORMAT_PALETTE8_RGBA5551);
}

int oglMapInternalTextureFormatAndType(unsigned int internalFormat, unsigned int type)
{
    if (internalFormat == GL_RGB &&
        (type == GL_UNSIGNED_SHORT_5_6_5 ||
         type == GL_UNSIGNED_SHORT_4_4_4_4 ||
         type == GL_UNSIGNED_SHORT_5_5_5_1))
    {
        return GR_TEXTUREFORMAT_RGB565;
    }
    if (internalFormat != GL_RGBA)
        return oglMapInternalTextureFormat(internalFormat);
    if (type == GL_UNSIGNED_SHORT_4_4_4_4)
        return GR_TEXTUREFORMAT_RGBA4444;
    if (type == GL_UNSIGNED_SHORT_5_5_5_1)
        return GR_TEXTUREFORMAT_RGBA5551;
    return oglMapInternalTextureFormat(internalFormat);
}

int oglMapTextureTarget(unsigned int target)
{
    return target != GL_TEXTURE_2D;
}

OGL_BUFOBJTARGET oglMapBufObjTarget(unsigned int target)
{
    if (target == GL_ARRAY_BUFFER)
        return OGL_BUFOBJTARGET_ARRAY;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        return OGL_BUFOBJTARGET_ELEMENT_ARRAY;
    return OGL_BUFOBJTARGET_MAX;
}

OGL_BUFOBJUSAGE oglMapBufObjUsage(unsigned int usage)
{
    if (usage == GL_STATIC_DRAW)
        return OGL_BUFOBJUSAGE_STATIC_DRAW;
    if (usage == GL_DYNAMIC_DRAW)
        return OGL_BUFOBJUSAGE_DYNAMIC_DRAW;
    return OGL_BUFOBJUSAGE_MAX;
}

unsigned int oglUnmapBufObjUsage(OGL_BUFOBJUSAGE usage)
{
    if (usage == OGL_BUFOBJUSAGE_STATIC_DRAW)
        return GL_STATIC_DRAW;
    OGL_ASSERT_AT("oglUtil.inl", 1064, "usage == OGL_BUFOBJUSAGE_DYNAMIC_DRAW",
                  usage == OGL_BUFOBJUSAGE_DYNAMIC_DRAW);
    return GL_DYNAMIC_DRAW;
}

int oglMapWrapMode(unsigned int pname)
{
    if (pname == GL_REPEAT)
        return GR_WRAPMODE_REPEAT;
    if (pname == GL_CLAMP_TO_EDGE)
        return GR_WRAPMODE_CLAMP_TO_EDGE;
    return GR_WRAPMODE_MAX;
}

unsigned int oglUnmapWrapMode(int pname)
{
    switch (pname)
    {
    case GR_WRAPMODE_REPEAT:
        return GL_REPEAT;
    case GR_WRAPMODE_CLAMP_TO_EDGE:
        return GL_CLAMP_TO_EDGE;
    default:
        if (grAssert("HG_FALSE", "oglUtil.inl", 1107) != 0)
            grDie(0);
        return 0;
    }
}

int oglMinusOne(void)
{
    return -1;
}

void oglMappingTest(void)
{
    int mn, mx, i, j, found;

    mn = 0x10000;
    mx = 0;
    for (i = 0; i < 5; ++i)
    {
        mn = hgMin32(mn, s_imageFormats[i].src);
        mx = hgMax32(mx, s_imageFormats[i].src);
    }
    for (i = mn - 300; i < mx + 300; ++i)
    {
        found = 0;
        for (j = 0; j < 5; ++j)
        {
            if (i == s_imageFormats[j].src)
            {
                OGL_ASSERT_AT("oglUtil.inl", 1240,
                              "oglMapImageFormat(i) == s_imageFormats[j].dst",
                              oglMapImageFormat((unsigned int)i) == s_imageFormats[j].dst);
                found = 1;
                break;
            }
        }
        if (found == 0 && oglMinusOne() != GR_IMAGEFORMAT_MAX)
        {
            OGL_ASSERT_AT("oglUtil.inl", 1240,
                          "oglMapImageFormat(i) == GR_IMAGEFORMAT_MAX",
                          oglMapImageFormat((unsigned int)i) == GR_IMAGEFORMAT_MAX);
        }
    }

    mn = 0x10000;
    mx = 0;
    for (i = 0; i < 6; ++i)
    {
        mn = hgMin32(mn, s_stencilOps[i].src);
        mx = hgMax32(mx, s_stencilOps[i].src);
    }
    for (i = mn - 300; i < mx + 300; ++i)
    {
        found = 0;
        for (j = 0; j < 6; ++j)
        {
            if (i == s_stencilOps[j].src)
            {
                OGL_ASSERT_AT("oglUtil.inl", 1241,
                              "oglMapStencilOp(i) == s_stencilOps[j].dst",
                              oglMapStencilOp((unsigned int)i) == s_stencilOps[j].dst);
                found = 1;
                break;
            }
        }
        if (found == 0 && oglMinusOne() != GR_STENCILOP_MAX)
        {
            OGL_ASSERT_AT("oglUtil.inl", 1241,
                          "oglMapStencilOp(i) == GR_STENCILOP_MAX",
                          oglMapStencilOp((unsigned int)i) == GR_STENCILOP_MAX);
        }
    }

    mn = 0x10000;
    mx = 0;
    for (i = 0; i < 4; ++i)
    {
        mn = hgMin32(mn, s_materialColors[i].src);
        mx = hgMax32(mx, s_materialColors[i].src);
    }
    for (i = mn - 300; i < mx + 300; ++i)
    {
        found = 0;
        for (j = 0; j < 4; ++j)
        {
            if (i == s_materialColors[j].src)
            {
                OGL_ASSERT_AT("oglUtil.inl", 1242,
                              "oglMapMaterialColor(i) == s_materialColors[j].dst",
                              oglMapMaterialColor((unsigned int)i) == s_materialColors[j].dst);
                found = 1;
                break;
            }
        }
        if (found == 0 && oglMinusOne() != GR_MATERIALCOLOR_MAX)
        {
            OGL_ASSERT_AT("oglUtil.inl", 1242,
                          "oglMapMaterialColor(i) == GR_MATERIALCOLOR_MAX",
                          oglMapMaterialColor((unsigned int)i) == GR_MATERIALCOLOR_MAX);
        }
    }

    mn = 0x10000;
    mx = 0;
    for (i = 0; i < 16; ++i)
    {
        mn = hgMin32(mn, s_logicOps[i].src);
        mx = hgMax32(mx, s_logicOps[i].src);
    }
    for (i = mn - 300; i < mx + 300; ++i)
    {
        found = 0;
        for (j = 0; j < 16; ++j)
        {
            if (i == s_logicOps[j].src)
            {
                OGL_ASSERT_AT("oglUtil.inl", 1243,
                              "oglMapLogicOp(i) == s_logicOps[j].dst",
                              oglMapLogicOp((unsigned int)i) == s_logicOps[j].dst);
                found = 1;
                break;
            }
        }
        if (found == 0 && oglMinusOne() != GR_LOGICOP_MAX)
        {
            OGL_ASSERT_AT("oglUtil.inl", 1243,
                          "oglMapLogicOp(i) == GR_LOGICOP_MAX",
                          oglMapLogicOp((unsigned int)i) == GR_LOGICOP_MAX);
        }
    }

    mn = 0x10000;
    mx = 0;
    for (i = 0; i < 5; ++i)
    {
        mn = hgMin32(mn, s_texEnvModes[i].src);
        mx = hgMax32(mx, s_texEnvModes[i].src);
    }
    for (i = mn - 300; i < mx + 300; ++i)
    {
        found = 0;
        for (j = 0; j < 5; ++j)
        {
            if (i == s_texEnvModes[j].src)
            {
                OGL_ASSERT_AT("oglUtil.inl", 1244,
                              "oglMapTexEnvMode(i) == s_texEnvModes[j].dst",
                              oglMapTexEnvMode((unsigned int)i) == s_texEnvModes[j].dst);
                found = 1;
                break;
            }
        }
        if (found == 0 && oglMinusOne() != GR_TEXENVMODE_MAX)
        {
            OGL_ASSERT_AT("oglUtil.inl", 1244,
                          "oglMapTexEnvMode(i) == GR_TEXENVMODE_MAX",
                          oglMapTexEnvMode((unsigned int)i) == GR_TEXENVMODE_MAX);
        }
    }

    mn = 0x10000;
    mx = 0;
    for (i = 0; i < 5; ++i)
    {
        mn = hgMin32(mn, s_internalTextureFormats[i].src);
        mx = hgMax32(mx, s_internalTextureFormats[i].src);
    }
    for (i = mn - 300; i < mx + 300; ++i)
    {
        found = 0;
        for (j = 0; j < 5; ++j)
        {
            if (i == s_internalTextureFormats[j].src)
            {
                OGL_ASSERT_AT("oglUtil.inl", 1245,
                              "oglMapInternalTextureFormat(i) == s_internalTextureFormats[j].dst",
                              oglMapInternalTextureFormat((unsigned int)i) == s_internalTextureFormats[j].dst);
                found = 1;
                break;
            }
        }
        if (found == 0 && oglMinusOne() != GR_TEXTUREFORMAT_MAX)
        {
            OGL_ASSERT_AT("oglUtil.inl", 1245,
                          "oglMapInternalTextureFormat(i) == GR_TEXTUREFORMAT_MAX",
                          oglMapInternalTextureFormat((unsigned int)i) == GR_TEXTUREFORMAT_MAX);
        }
    }
}

/* --- Texture format / memory-image / unit helpers -------------------------- */

int oglIsValidTextureType(unsigned int type)
{
    return type == GL_UNSIGNED_BYTE ||
           type == GL_UNSIGNED_SHORT_5_6_5 ||
           type == GL_UNSIGNED_SHORT_4_4_4_4 ||
           type == GL_UNSIGNED_SHORT_5_5_5_1;
}

int oglIsCompatibleTextureType(unsigned int format, unsigned int type)
{
    switch (format)
    {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
        return type == GL_UNSIGNED_BYTE;
    case GL_RGB:
        return type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5;
    case GL_RGBA:
        return type == GL_UNSIGNED_BYTE ||
               type == GL_UNSIGNED_SHORT_4_4_4_4 ||
               type == GL_UNSIGNED_SHORT_5_5_5_1;
    default:
        return 0;
    }
}

int oglIsCompatibleInternalFormat(unsigned int format, int texFormat)
{
    switch (format)
    {
    case GL_ALPHA:
        return texFormat == GR_TEXTUREFORMAT_ALPHA8;
    case GL_RGB:
        return texFormat == GR_TEXTUREFORMAT_LUMINANCE8 ||
               texFormat == GR_TEXTUREFORMAT_RGB8 ||
               texFormat == GR_TEXTUREFORMAT_RGB565;
    case GL_RGBA:
        return 1;
    case GL_LUMINANCE:
        return texFormat == GR_TEXTUREFORMAT_LUMINANCE8;
    case GL_LUMINANCE_ALPHA:
        return texFormat == GR_TEXTUREFORMAT_ALPHA8 ||
               texFormat == GR_TEXTUREFORMAT_LUMINANCE8 ||
               texFormat == GR_TEXTUREFORMAT_LUMINANCE8_ALPHA8;
    default:
        return 0;
    }
}

void oglFillMemoryImageFormat(grMemoryImage_s *image, unsigned int format, unsigned int type)
{
    int dataType;

    OGL_ASSERT_AT("oglUtil.inl", 1334, "image", image != NULL);
    OGL_ASSERT_AT("oglUtil.inl", 1335, "oglIsCompatibleTextureType(format, type)",
                  oglIsCompatibleTextureType(format, type) != 0);
    dataType = oglMapDataType(type);
    if (dataType == GR_DATATYPE_MAX)
    {
        oglFillDescriptor(&image->descriptor, format, type);
        image->bytesPerPixel = image->descriptor.colorBytes;
        image->useDescriptor = 1;
    }
    else
    {
        int imageFormat = oglMapImageFormat(format);
        OGL_ASSERT_AT("oglUtil.inl", 1342, "dataType != GR_DATATYPE_BOOL",
                      dataType != GR_DATATYPE_BOOL);
        OGL_ASSERT_AT("oglUtil.inl", 1343, "imageFormat != GR_IMAGEFORMAT_MAX",
                      imageFormat != GR_IMAGEFORMAT_MAX);
        image->dataType = dataType;
        image->imageFormat = imageFormat;
        image->bytesPerPixel = grGetDataTypeSize(dataType) *
                               grGetImageFormatNumComponents(imageFormat);
        image->useDescriptor = 0;
    }
}

void oglUpdateTextureUnit(oglContext_s *context, int texUnit)
{
    int texTarget;

    OGL_ASSERT_AT("oglUtil.inl", 1549, "context", context != NULL);
    OGL_ASSERT_AT("oglUtil.inl", 1550, "hgInRange32(texUnit, 0, GR_MAX_TEXTURE_UNITS-1)",
                  hgInRange32(texUnit, 0, GR_MAX_TEXTURE_UNITS - 1) != 0);
    for (texTarget = GR_TEXTURETARGET_MAX - 1; texTarget >= 0; --texTarget)
    {
        _grTexture *texture = oglGetTexture(context->textureManager,
                                            context->activeTextures[texTarget][texUnit]);
        if (texture != NULL && context->textureEnabled[texUnit][texTarget] != 0)
        {
            grSetActiveTexture(context->gerbera, texUnit, texture);
            grSetCapability(context->gerbera, GR_CAPABILITY_TEXTURE_0 + texUnit, 1);
            return;
        }
    }
    grSetCapability(context->gerbera, GR_CAPABILITY_TEXTURE_0 + texUnit, 0);
}

_grTexture *oglGetActiveTexture(oglContext_s *context, int texTarget)
{
    OGL_ASSERT_AT("oglUtil.inl", 1583, "context", context != NULL);
    OGL_ASSERT_AT("oglUtil.inl", 1584, "texTarget == GR_TEXTURETARGET_2D",
                  texTarget == GR_TEXTURETARGET_2D);
    return oglGetTexture(context->textureManager,
                         context->activeTextures[texTarget][context->activeTexture]);
}

void oglBindTexture(oglContext_s *context, int texUnit, int texTarget, unsigned int texIndex)
{
    _grTexture *texture;
    int oldTexTarget;

    OGL_ASSERT_AT("oglUtil.inl", 1603, "context", context != NULL);
    OGL_ASSERT_AT("oglUtil.inl", 1604, "hgInRange32(texUnit, 0, GR_MAX_TEXTURE_UNITS-1)",
                  hgInRange32(texUnit, 0, GR_MAX_TEXTURE_UNITS - 1) != 0);
    OGL_ASSERT_AT("oglUtil.inl", 1605, "hgInRange32((int)texTarget, 0, GR_TEXTURETARGET_MAX-1)",
                  hgInRange32(texTarget, 0, GR_TEXTURETARGET_MAX - 1) != 0);
    OGL_ASSERT_AT("oglUtil.inl", 1606, "oglIsTexture(context->textureManager, texIndex)",
                  oglIsTexture(context->textureManager, texIndex) != 0);

    texture = oglGetTexture(context->textureManager, texIndex);
    oldTexTarget = grGetTextureTarget(texture);
    if (oldTexTarget == GR_TEXTURETARGET_MAX)
    {
        grSetTextureTarget(texture, texTarget);
        oldTexTarget = texTarget;
    }
    if (oldTexTarget == texTarget)
    {
        int nameAsInt = (int)texIndex;
        context->activeTextures[texTarget][texUnit] = texIndex;
        oglUpdateTextureUnit(context, texUnit);
        oglSetGetterValueInt(context, texUnit, GL_TEXTURE_BINDING_2D, &nameAsInt);
    }
    else
    {
        oglSetError(context, OGL_ERROR_INVALID_OPERATION);
    }
}

unsigned int oglMapBufferMask(unsigned int buf)
{
    unsigned int res = (buf & GL_COLOR_BUFFER_BIT) != 0;
    if ((buf & GL_DEPTH_BUFFER_BIT) != 0)
        res |= 2u;
    if ((buf & GL_STENCIL_BUFFER_BIT) != 0)
        res |= 4u;
    return res;
}

int oglIsScalarFogParam(unsigned int pname)
{
    return hgInRange32((int)pname, GL_FOG_DENSITY, GL_FOG_MODE) != 0;
}

int oglIsScalarPointParameter(unsigned int pname)
{
    return pname == GL_POINT_SIZE_MIN ||
           pname == GL_POINT_SIZE_MAX ||
           pname == GL_POINT_FADE_THRESHOLD_SIZE;
}

void oglSetClientState(oglContext_s *context, unsigned int array, int state)
{
    int cap = oglMapClientStateCapability(context, array);
    if (cap == GR_CAPABILITY_MAX)
    {
        oglSetError(context, OGL_ERROR_INVALID_ENUM);
    }
    else
    {
        int offset = (array == GL_TEXTURE_COORD_ARRAY) ? context->clientActiveTexture : 0;
        grSetCapability(context->gerbera, cap, state);
        oglSetGetterValueBool(context, offset, array, &state);
    }
}

void oglMaterialInternal(oglContext_s *context, unsigned int face, unsigned int pname,
                         const void *params, OGL_INPUTTYPE inputType)
{
    if (face != GL_FRONT_AND_BACK)
    {
        oglSetError(context, OGL_ERROR_INVALID_ENUM);
        return;
    }
    if (pname == GL_AMBIENT_AND_DIFFUSE)
    {
        oglMaterialInternal(context, GL_FRONT_AND_BACK, GL_AMBIENT, params, inputType);
        oglMaterialInternal(context, GL_FRONT_AND_BACK, GL_DIFFUSE, params, inputType);
        return;
    }
    if (pname - GL_AMBIENT <= 2u || pname == GL_EMISSION)
    {
        int matColor = oglMapMaterialColor(pname);
        _XColor3 col;
        float fCol[4];
        int c;

        oglInputToXColor3(&col, params, inputType);
        grSetMaterialColor(context->gerbera, matColor, &col);
        if (pname == GL_DIFFUSE)
            grSetMaterialAlpha(context->gerbera, oglInputAlpha(params, inputType));
        oglInputToFloatv(fCol, params, inputType, 4);
        for (c = 3; c >= 0; --c)
            context->materialColors[matColor][c] = fCol[c];
    }
    else if (pname == GL_SHININESS)
    {
        int shininess = oglInputToIScalarDirect(params, inputType);
        if (oglTestInput(params, 0, inputType) >= 0 &&
            IScalar_cmple(shininess, IScalar_immediate(128.0f)) != 0)
        {
            grSetMaterialShininess(context->gerbera, shininess);
        }
        else
        {
            oglSetError(context, OGL_ERROR_INVALID_VALUE);
        }
    }
    else
    {
        oglSetError(context, OGL_ERROR_INVALID_ENUM);
    }
}

void oglLightInternal(oglContext_s *context, unsigned int lname, unsigned int pname,
                      const void *params, OGL_INPUTTYPE inputType)
{
    Gerbera_s *gerbera = context->gerbera;
    int light = (int)(lname - GL_LIGHT0);
    int scalarVal;

    if (hgInRange32(light, 0, GR_MAX_LIGHTS - 1) == 0)
    {
        oglSetError(context, OGL_ERROR_INVALID_ENUM);
        return;
    }

    scalarVal = oglInputToIScalarDirect(params, inputType);
    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    {
        int colType = oglMapMaterialColor(pname);
        _XColor3 col;

        oglInputToXColor3(&col, params, inputType);
        grSetLightColor(gerbera, light, colType, &col);
        context->lightAlphas[light][colType] =
            oglInputToFloatDirect((const unsigned char *)params + 12, inputType);
        return;
    }
    case GL_POSITION:
    {
        _IFVector4 pos;
        _IVertexTransform xform;

        oglInputToIFVector4Direct(&pos, params, inputType);
        IVertexTransform_copy(&xform, oglGetMatrix(context, GR_MATRIXMODE_MODELVIEW));
        IVertexTransform_mulfv4s(&pos, &xform, &pos);
        grSetLightPosition(gerbera, light, &pos);
        return;
    }
    case GL_SPOT_DIRECTION:
    {
        _IFVector4 dir4;
        _IMatrix4x4 mvT;
        int f3[3];
        _IFVector3 dir3;
        _IVector3 iv3;
        int norm3[3];

        oglInputToIFVector4W0(&dir4, params, inputType);
        IMatrix4x4_transpose(&mvT, oglGetMatrix(context, GR_MATRIXMODE_MODELVIEW));
        IMatrix4x4_mulfv4(&dir4, &mvT, &dir4);
        f3[0] = IFVector4_getf(&dir4, 0);
        f3[1] = IFVector4_getf(&dir4, 1);
        f3[2] = IFVector4_getf(&dir4, 2);
        IFVector3_setfv3(&dir3, f3);
        IFVector3_tov3(&iv3, &dir3);
        XNormal3_normalize(norm3, iv3.m);
        grSetLightSpotDirection(gerbera, light, norm3, &dir3);
        return;
    }
    case GL_SPOT_EXPONENT:
        if (oglTestInput(params, 0, inputType) < 0 ||
            IScalar_cmple(scalarVal, IScalar_immediate(128.0f)) == 0)
        {
            oglSetError(context, OGL_ERROR_INVALID_VALUE);
        }
        else
        {
            grSetLightExponent(gerbera, light, scalarVal);
        }
        return;
    case GL_SPOT_CUTOFF:
        if ((oglTestInput(params, 0, inputType) < 0 ||
             IScalar_cmple(scalarVal, IScalar_intToScalar(90)) == 0) &&
            IScalar_cmpeq(scalarVal, IScalar_immediate(180.0f)) == 0)
        {
            oglSetError(context, OGL_ERROR_INVALID_VALUE);
        }
        else
        {
            grSetLightSpotCutoff(gerbera, light, scalarVal);
        }
        return;
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        if (oglTestInput(params, 0, inputType) < 0)
            oglSetError(context, OGL_ERROR_INVALID_VALUE);
        else
            grSetLightAttenuation(gerbera, light, (int)(pname - GL_CONSTANT_ATTENUATION), scalarVal);
        return;
    default:
        oglSetError(context, OGL_ERROR_INVALID_ENUM);
        return;
    }
}

int oglIsScalarLightParameter(unsigned int pname)
{
    return hgInRange32((int)pname, GL_SPOT_EXPONENT, GL_QUADRATIC_ATTENUATION);
}

int oglIsScalarLightModelParameter(unsigned int pname)
{
    return pname == GL_LIGHT_MODEL_TWO_SIDE;
}

int oglIsScalarTexEnvParam(unsigned int pname)
{
    return pname != GL_TEXTURE_ENV_COLOR;
}

int oglIsScalarTexParam(unsigned int param)
{
    if (hgInRange32((int)param, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_T) == 0)
        return param == GL_GENERATE_MIPMAP;
    return 1;
}
