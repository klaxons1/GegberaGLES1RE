/* grState.h - the Gerbera driver context (decompile type Gerbera_s).
 *
 * The original defs.h was lost; the struct is reconstructed field-by-field
 * from the decompiled core code. Fields land here as the functions that use
 * them are ported. Field ORDER in memory is not pinned by the decompile
 * (everything is accessed by name); only the field SET is faithful.
 *
 * Capability enum values: only 41..48 (the eight lights, from grSetCapability
 * `cap - 41`) are pinned so far; the rest keep decompile member names with
 * unknown numbers (they pin when the ogl mapping tables are extracted).
 * The affectingCapabilities bit tables below are verbatim from the binary
 * (lines 5700/5706), indexed by cap >> 5 / cap & 0x1F.
 */
#ifndef GR_STATE_H
#define GR_STATE_H

#include "hg/hgDefs.h"
#include "gerbera/grDefs.h"
#include "gerbera/grTexture.h"
#include "hg/hgInt32.h"
#include "hg/hgFloat.h"
#include "imath/imApi.h"

HG_EXTERN_C_BEGIN

#define GR_CAPABILITY_MAX 54
#define GR_MAX_LIGHTS 8
#define GR_MAX_USER_CLIP_PLANES 6
#define GR_MAX_DISPLAY_DIMENSION 2048
#define GR_MAX_VIEWPORT_DIMENSION 2048
#define GR_CLEAR_BUFFER_SIZE 4
#define GR_COLORMASK_FULL 0xFu
#define GR_SIDEMASK_BOTH 3u

/* winding (grSetCullMode) */
enum GR_WINDING_e
{
    GR_WINDING_CW = 0,
    GR_WINDING_CCW = 1
};

/* compare functions (grSetAlphaFunc/grSetStencilFunc/grSetDepthFunc) */
enum GR_FUNC_e
{
    GR_FUNC_NEVER = 0,
    GR_FUNC_LESS = 1,
    GR_FUNC_LEQUAL = 2,
    GR_FUNC_GREATER = 3,
    GR_FUNC_GEQUAL = 4,
    GR_FUNC_EQUAL = 5,
    GR_FUNC_NOTEQUAL = 6,
    GR_FUNC_ALWAYS = 7,
    GR_FUNC_MAX = 8
};

enum GR_STENCILOP_e
{
    GR_STENCILOP_ZERO = 0,
    GR_STENCILOP_KEEP = 1,
    GR_STENCILOP_REPLACE = 2,
    GR_STENCILOP_INCR = 3,
    GR_STENCILOP_DECR = 4,
    GR_STENCILOP_INVERT = 5,
    GR_STENCILOP_MAX = 6
};

enum GR_LOGICOP_e
{
    GR_LOGICOP_CLEAR = 0,
    GR_LOGICOP_AND = 1,
    GR_LOGICOP_AND_REVERSE = 2,
    GR_LOGICOP_COPY = 3,
    GR_LOGICOP_AND_INVERTED = 4,
    GR_LOGICOP_NOOP = 5,
    GR_LOGICOP_XOR = 6,
    GR_LOGICOP_OR = 7,
    GR_LOGICOP_NOR = 8,
    GR_LOGICOP_EQUIV = 9,
    GR_LOGICOP_INVERT = 10,
    GR_LOGICOP_OR_REVERSE = 11,
    GR_LOGICOP_COPY_INVERTED = 12,
    GR_LOGICOP_OR_INVERTED = 13,
    GR_LOGICOP_NAND = 14,
    GR_LOGICOP_SET = 15,
    GR_LOGICOP_MAX = 16
};

enum GR_BLENDFACTOR_e
{
    GR_BLENDFACTOR_ZERO = 0,
    GR_BLENDFACTOR_ONE = 1,
    GR_BLENDFACTOR_MAX = 15        /* 0..14 valid (grSetBlendFunc assert) */
};

enum GR_FOGMODE_e
{
    GR_FOGMODE_NONE = 0,
    GR_FOGMODE_LINEAR = 1,
    GR_FOGMODE_EXP = 2,
    GR_FOGMODE_MAX = 3
};

/* capabilities (values unknown except the light block 41..48) */
enum GR_CAPABILITY_e
{
    GR_CAPABILITY_DITHER = 0,             /* value not pinned yet */
    GR_CAPABILITY_SCISSOR_TEST = 1,       /* value not pinned yet */
    GR_CAPABILITY_LIGHT_MODEL_TWO_SIDE = 2, /* value not pinned yet */
    GR_CAPABILITY_COLOR_MATERIAL = 3,     /* value not pinned yet */
    GR_CAPABILITY_RESCALE_NORMAL = 4,     /* value not pinned yet */
    GR_CAPABILITY_LIGHT0 = 41,            /* lights occupy 41..48 */
    GR_CAPABILITY_LIGHT7 = 48,
    GR_CAPABILITY_MAX_VALUE = 54
};

/* light record (decompile cv_anon_1748). The decompile reaches the fields
 * through a mix of named accesses and byte-offset aliases (txMin/txMax/fx1
 * are Hex-Rays union guesses); each alias maps 1:1 onto the clean fields
 * below: colors[i]=txMax[3i], spotDirectionNegated=&txMin[1] of elem +1,
 * orgSpotDirection=fx1, orgPosition=&txMax[1] of elem 0, exponent=txMin[0]
 * of elem +2, attenuation=&txMin[1] of elem +2, cosSpotCutoff=txMax[1] of
 * elem +2, spotCutoff=tyMin[1] of elem 0. The +20 lightMask byte offset is
 * from grVertexPipeline (assert "pLight->lightMask == lightMask").
 * lightMask bits: 1=ambient nonzero, 2=diffuse nonzero, 3=specular nonzero
 * (from grUpdateBaseColor's |= (test & 2)/(test & 4)/(test & 8)). */
typedef struct _grLight
{
    int positional;               /* IFVector4_tov3 return: w != 0 */
    _IVector3 positionXYZ[1];     /* shared-exp position, w divided out */
    int nVP[3];                   /* normalized position (10.21), directional lights */
    _IFVector4 orgPosition;       /* original float position */
    int lightMask;
    _XColor3 colors[3];           /* ambient, diffuse, specular */
    _XColor3 premul_colors[3];    /* colors * material (vertex pipeline) */
    int spotDirectionNegated[3];  /* negated spot direction (10.21) */
    _IFVector3 orgSpotDirection;  /* original float spot direction */
    int exponent;                 /* XScalar */
    _IFVector3 attenuation;       /* constant, linear, quadratic (floats) */
    int cosSpotCutoff;            /* XScalar: max(cos(radians(cutoff)), 0) */
    int spotCutoff;               /* original cutoff (IScalar, degrees) */
} _grLight;

/* material color slots (grSetMaterialColor; 0..3 valid, GR_MATERIALCOLOR_MAX=4).
 * AMBIENT and EMISSION are named in the decompile; numeric order unpinned. */
enum GR_MATERIALCOLOR_e
{
    GR_MATERIALCOLOR_AMBIENT = 0,     /* value not pinned yet */
    GR_MATERIALCOLOR_EMISSION = 3,    /* value not pinned yet */
    GR_MATERIALCOLOR_MAX = 4
};

/* material state (decompile cv_anon_174D, reconstructed from byte offsets:
 * colors[4] _XColor3 at ints 0..11, XScalar shininess at int 12, alpha
 * unsigned short at byte 52) */
typedef struct _grMaterial
{
    _XColor3 colors[4];
    int shininess;                /* XScalar */
    unsigned short alpha;
} _grMaterial;

/* scissor / viewport rectangles; the pipeline consumes these as scale/offset
 * triplets (the decompile's union member normal[3]) */
typedef struct _grViewportParams
{
    int normal[3];
} _grViewportParams;

/* the driver context. Only fields used by ported functions are listed;
 * the set grows with the port (census: 119 distinct state-> fields). */
typedef struct Gerbera_s
{
    /* --- capabilities --------------------------------------------------- */
    unsigned char capabilities[GR_CAPABILITY_MAX];

    /* --- rectangles ------------------------------------------------------ */
    _grRect scissorRectangle;
    _grRect viewportRectangle;
    _grRect surfaceScissor;
    _grRect renderScissor;
    int scissorDirty;
    int rejectAllStateDirty;
    int pixelPipelineDirty;
    int vertexPipelineDirty;

    /* --- viewport / depth range ------------------------------------------ */
    _grViewportParams viewportScale;
    _grViewportParams viewportOffset;
    int zNear;
    int zFar;

    /* --- draw surface ----------------------------------------------------- */
    _grDrawSurface drawSurface;
    int useDithering;

    /* --- clears ----------------------------------------------------------- */
    unsigned int clearColor;
    int clearDepth;
    unsigned int clearStencil;
    int clearInfoIndex;

    /* --- per-pixel fixed-function state ----------------------------------- */
    unsigned int colorMask;
    int depthMask;
    unsigned int stencilWriteMask;
    int alphaFunc;
    int alphaReference;
    int stencilFunc;
    unsigned int stencilReference;
    unsigned int stencilTestMask;
    int stencilFail;
    int stencilZFail;
    int stencilZPass;
    int depthFunc;
    int logicOp;
    int srcBlendFactor;
    int dstBlendFactor;
    int depthOffsetFactor;
    int depthOffsetUnits;

    /* --- fog --------------------------------------------------------------- */
    int fogMode;
    int fogDensity;
    int fogStart;
    int fogEnd;
    int fogOORange;
    unsigned int fogAtZero;
    unsigned int fogColor;

    /* --- gradients ---------------------------------------------------------- */
    int screenRectGradients;

    /* --- lighting bookkeeping ----------------------------------------------- */
    int lightActiveDirtyMask;
    unsigned int lightActiveMask;
    unsigned int lightColorDirtyMask;
    unsigned int lightTypeDirtyMask;
    unsigned char normalsToEyePalDirty[9];
    int normalsToEyeDirty;
    _grLight lights[GR_MAX_LIGHTS];

    /* --- culling -------------------------------------------------------------- */
    int frontFace;
    unsigned int cullMask;
    unsigned char cullNegative;
    unsigned char lightingSideMask;

    /* --- global ambient -------------------------------------------------------- */
    _XColor3 ambientColor;
    int ambientLightSumDirty;

    /* --- material ---------------------------------------------------------------- */
    _grMaterial material;
    int baseColorDirty;
} Gerbera_s;

/* --- capabilities (grUtil.h) --------------------------------------------- */
int  grTestCapability(Gerbera_s *state, int cap);
void grEnableCapability(Gerbera_s *state, int cap);
void grDisableCapability(Gerbera_s *state, int cap);
int  grIsDepthOffsetEnabled(Gerbera_s *state, int primitiveType);

/* --- grApiSmall.inl: cull / scissor / viewport / caps --------------------- */
void grUpdateCullMode(Gerbera_s *state);
void grSetDepthRange(Gerbera_s *state, int n, int f);
void grUpdateScissor(Gerbera_s *state);
void grSetScissor(Gerbera_s *state, int x0, int x1, int y0, int y1);
void grSetViewport(Gerbera_s *state, int x0, int x1, int y0, int y1);
int  grCapChangesRejectAllState(int cap);
int  grCapChangesPixelPipeline(int cap);
void grSetCapability(Gerbera_s *state, int cap, int status);
int  grGetCapability(Gerbera_s *state, int cap);
void grUpdateDither(Gerbera_s *state);

/* --- grApiSmall.inl: clears ----------------------------------------------- */
void grSetClearColor(Gerbera_s *state, unsigned int color);
void grSetClearDepth(Gerbera_s *state, int depth);
void grSetClearStencil(Gerbera_s *state, unsigned int s);

/* --- grApiSmall.inl: per-pixel state -------------------------------------- */
void grSetCullMode(Gerbera_s *state, int frontFace, unsigned int cullMask);
void grSetFogMode(Gerbera_s *state, int fogMode);
void grSetFogDensity(Gerbera_s *state, int density);
void grSetFogRange(Gerbera_s *state, int fogStart, int fogEnd);
void grSetFogColor(Gerbera_s *state, unsigned int fogColor);
void grSetRectGradients(Gerbera_s *state, int gradients);
void grSetMaterialColor(Gerbera_s *state, int component, _XColor3 *color);
void grSetMaterialAlpha(Gerbera_s *state, unsigned short alpha);
void grSetMaterialShininess(Gerbera_s *state, int shininess);
int  grGetMaterialShininess(Gerbera_s *state);
void grSetLightPosition(Gerbera_s *state, int ndx, _IFVector4 *position);
_IFVector4 *grGetLightOrgPosition(Gerbera_s *state, int ndx);
void grSetLightSpotDirection(Gerbera_s *state, int ndx, int *direction, _IFVector3 *orgDirection);
_IFVector3 *grGetLightOrgDirection(Gerbera_s *state, int ndx);
void grSetLightColor(Gerbera_s *state, int ndx, int component, _XColor3 *color);
void grGetLightColor(Gerbera_s *state, int ndx, int component, _XColor3 *color);
void grSetLightAttenuation(Gerbera_s *state, int ndx, int component, int value);
int  grGetLightAttenuation(Gerbera_s *state, int ndx, int component);
void grSetLightExponent(Gerbera_s *state, int ndx, int exponent);
int  grGetLightExponent(Gerbera_s *state, int ndx);
void grSetLightSpotCutoff(Gerbera_s *state, int ndx, int cutoff);
int  grGetLightSpotCutoff(Gerbera_s *state, int ndx);
unsigned int grTestLightColor(_grLight *light, int component);
unsigned int grPrecalcLightColor(_grLight *light, int component, _XColor3 *materialColor);
void grSetGlobalAmbient(Gerbera_s *state, _XColor3 *ambient);
void grSetAlphaFunc(Gerbera_s *state, int func, int ref);
void grSetStencilFunc(Gerbera_s *state, int func, unsigned int ref, unsigned int mask);
void grGetDrawBufferDescriptor(Gerbera_s *state, _grSurfaceDescriptor *desc);
void grSetDepthOffset(Gerbera_s *state, int factor, int units);
void grSetStencilOp(Gerbera_s *state, int sFail, int dFail, int dPass);
void grSetDepthFunc(Gerbera_s *state, int func);
void grSetLogicOp(Gerbera_s *state, int logicOp);
void grSetBlendFunc(Gerbera_s *state, int srcFactor, int dstFactor);
void grSetColorMask(Gerbera_s *state, unsigned int mask);
void grSetDepthMask(Gerbera_s *state, int mask);
void grSetStencilMask(Gerbera_s *state, unsigned int mask);

HG_EXTERN_C_END

#endif /* GR_STATE_H */
