/* grApiSmall.c - state setters (port of the decompiled grApiSmall.inl,
 * chunk 1: cull/scissor/viewport/capabilities/clears/fog/alpha/stencil/
 * depth/blend/masks/matrix/user-clip-planes).
 *
 * Assert sites keep the original grApiSmall.inl file/line strings.
 */
#include "gerbera/grState.h"
#include "gerbera/grRGBA.h"
#include "hg/hgInt32.h"

#define GR_API_CHECK(cond, line)                 \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, "grApiSmall.inl", (line)) != 0) \
            grDie(0);                            \
    } while (0)

/* verbatim from the binary (docs/libGLES_CM.dll.c:5700/5706) */
int grCapChangesRejectAllState_affectingCapabilities[] = { 98306 };
int grCapChangesPixelPipeline_affectingCapabilities[15] =
{
    32766,
    0,
    1025536042,
    1210589245,
    1852405063,
    808007796,
    1966223736,
    545029152,
    1025536042,
    1210589245,
    1852405063,
    808007796,
    1966618488,
    1147627520,
    779314789
};

/* --- capabilities (grUtil.h) ---------------------------------------------- */

int grTestCapability(Gerbera_s *state, int cap)
{
    GR_API_CHECK(hgInRange32(cap, 0, GR_CAPABILITY_MAX - 1), 35);
    return state->capabilities[cap];
}

void grEnableCapability(Gerbera_s *state, int cap)
{
    GR_API_CHECK(hgInRange32(cap, 0, GR_CAPABILITY_MAX - 1), 41);
    state->capabilities[cap] = 1;
}

void grDisableCapability(Gerbera_s *state, int cap)
{
    GR_API_CHECK(hgInRange32(cap, 0, GR_CAPABILITY_MAX - 1), 47);
    state->capabilities[cap] = 0;
}

int grIsDepthOffsetEnabled(Gerbera_s *state, int primitiveType)
{
    GR_API_CHECK(state != NULL, 148);
    return grTestCapability(state, primitiveType + 11);
}

/* --- cull ------------------------------------------------------------------ */

void grUpdateCullMode(Gerbera_s *state)
{
    unsigned char cullNegative;
    unsigned char lightingSideMask;

    GR_API_CHECK(state != NULL, 37);
    cullNegative = (state->cullMask & 1) != 0;
    if (state->frontFace == 1)
        cullNegative = (state->cullMask & 1) == 0;
    state->cullNegative = cullNegative;
    lightingSideMask = 1;
    if (grTestCapability(state, GR_CAPABILITY_LIGHT_MODEL_TWO_SIDE) != 0)
        lightingSideMask = 3;
    state->lightingSideMask = lightingSideMask;
}

/* --- depth range / scissor / viewport -------------------------------------- */

void grSetDepthRange(Gerbera_s *state, int n, int f)
{
    int zNear, zFar;

    GR_API_CHECK(state != NULL, 81);
    zNear = IScalar_toFixedHO(IScalar_clampUnit(n), 30);
    zFar = IScalar_toFixedHO(IScalar_clampUnit(f), 30);
    state->viewportScale.normal[2] = (zFar - zNear) >> 1;
    state->viewportOffset.normal[2] = (zNear + zFar + 1) >> 1;
    state->pixelPipelineDirty = 1;
    state->zNear = n;
    state->zFar = f;
}

void grUpdateScissor(Gerbera_s *state)
{
    int x0, y0, x1, y1;
    int x0a, y0a, x1a, y1a;

    GR_API_CHECK(state != NULL, 131);
    x0 = 0;
    y0 = 0;
    x1 = state->drawSurface.width;
    y1 = state->drawSurface.height;
    if (grTestCapability(state, GR_CAPABILITY_SCISSOR_TEST) != 0)
    {
        int x0b = hgMax32(0, state->scissorRectangle.x0);
        int y0b = hgMax32(0, state->scissorRectangle.y0);
        x1 = hgMin32(x1, state->scissorRectangle.x1);
        y1 = hgMin32(y1, state->scissorRectangle.y1);
        x0 = hgMin32(x0b, x1);
        y0 = hgMin32(y0b, y1);
    }
    grSetRectangle(&state->surfaceScissor, x0, x1, y0, y1);
    x0a = hgMax32(x0, state->viewportRectangle.x0);
    x1a = hgMin32(x1, state->viewportRectangle.x1);
    y0a = hgMax32(y0, state->viewportRectangle.y0);
    y1a = hgMin32(y1, state->viewportRectangle.y1);
    if (x0a > x1a || y0a > y1a)
    {
        y1a = 0;
        y0a = 0;
        x1a = 0;
        x0a = 0;
    }
    grSetRectangle(&state->renderScissor, x0a, x1a, y0a, y1a);
    GR_API_CHECK(grIsRectangleInRange(&state->renderScissor, 0, GR_MAX_DISPLAY_DIMENSION) != 0, 176);
    state->scissorDirty = 0;
    state->rejectAllStateDirty = 1;
}

void grSetScissor(Gerbera_s *state, int x0, int x1, int y0, int y1)
{
    GR_API_CHECK(state != NULL, 195);
    grSetRectangle(&state->scissorRectangle, x0, x1, y0, y1);
    grUpdateScissor(state);
}

void grSetViewport(Gerbera_s *state, int x0, int x1, int y0, int y1)
{
    GR_API_CHECK(state != NULL, 221);
    GR_API_CHECK(y0 <= y1 && x0 <= x1, 223);
    GR_API_CHECK(x1 - x0 <= GR_MAX_VIEWPORT_DIMENSION, 224);
    GR_API_CHECK(y1 - y0 <= GR_MAX_VIEWPORT_DIMENSION, 225);
    grSetRectangle(&state->viewportRectangle, x0, x1, y0, y1);
    state->viewportOffset.normal[0] = 16 * x0 + 8 * (x1 - x0) + 8;
    state->viewportOffset.normal[1] = 16 * y0 + 8 * (y1 - y0) + 8;
    state->viewportScale.normal[0] = imLsl32OF(x1 - x0, 6);
    state->viewportScale.normal[1] = imLsl32OF(y0 - y1, 6);
    grUpdateScissor(state);
}

/* --- capabilities ----------------------------------------------------------- */

int grCapChangesRejectAllState(int cap)
{
    GR_API_CHECK(hgInRange32(cap, 0, GR_CAPABILITY_MAX - 1), 285);
    return ((unsigned int)grCapChangesRejectAllState_affectingCapabilities[cap >> 5] >> (cap & 0x1F)) & 1;
}

int grCapChangesPixelPipeline(int cap)
{
    GR_API_CHECK(hgInRange32(cap, 0, GR_CAPABILITY_MAX - 1), 294);
    return ((unsigned int)grCapChangesPixelPipeline_affectingCapabilities[cap >> 5] >> (cap & 0x1F)) & 1;
}

void grSetCapability(Gerbera_s *state, int cap, int status)
{
    GR_API_CHECK(state != NULL, 303);
    GR_API_CHECK(hgInRange32(cap, 0, GR_CAPABILITY_MAX - 1), 304);
    GR_API_CHECK(grIsValidBool(status) != 0, 305);
    if (status != grTestCapability(state, cap))
    {
        int i;
        if (status != 0)
            grEnableCapability(state, cap);
        else
            grDisableCapability(state, cap);
        if (cap == GR_CAPABILITY_DITHER)
            grUpdateDither(state);
        if (cap == GR_CAPABILITY_SCISSOR_TEST)
            grUpdateScissor(state);
        if (hgInRange32(cap, 41, 48) != 0)
        {
            int ndx = cap - 41;
            state->lightActiveDirtyMask |= 1 << ndx;
            GR_API_CHECK(status == ((state->lightActiveMask & (1 << ndx)) == 0), 325);
            state->lightActiveMask ^= 1 << ndx;
        }
        if (cap == GR_CAPABILITY_COLOR_MATERIAL)
            state->lightColorDirtyMask = 0xFF;
        if (cap == GR_CAPABILITY_LIGHT_MODEL_TWO_SIDE)
            grUpdateCullMode(state);
        if (grCapChangesRejectAllState(cap) != 0)
            state->rejectAllStateDirty = 1;
        if (grCapChangesPixelPipeline(cap) != 0)
            state->pixelPipelineDirty = 1;
        if (cap == GR_CAPABILITY_RESCALE_NORMAL)
        {
            for (i = 8; i >= 0; --i)
                state->normalsToEyePalDirty[i] = 1;
            state->normalsToEyeDirty = 1;
        }
    }
}

int grGetCapability(Gerbera_s *state, int cap)
{
    GR_API_CHECK(state != NULL, 371);
    return grTestCapability(state, cap);
}

void grUpdateDither(Gerbera_s *state)
{
    unsigned char useDither = 0;

    if (grTestCapability(state, GR_CAPABILITY_DITHER) != 0 &&
        (((state->drawSurface.colorMask & 1) != 0 && hgInRange32(state->drawSurface.rgbaSizes[0], 1, 7) != 0) ||
         ((state->drawSurface.colorMask & 2) != 0 && hgInRange32(state->drawSurface.rgbaSizes[1], 1, 7) != 0) ||
         ((state->drawSurface.colorMask & 4) != 0 && hgInRange32(state->drawSurface.rgbaSizes[2], 1, 7) != 0) ||
         ((state->drawSurface.colorMask & 8) != 0 && hgInRange32(state->drawSurface.rgbaSizes[3], 1, 7) != 0)))
    {
        useDither = 1;
    }
    state->useDithering = useDither;
}

/* --- clears ------------------------------------------------------------------ */

void grSetClearColor(Gerbera_s *state, unsigned int color)
{
    GR_API_CHECK(state != NULL, 426);
    state->clearColor = color;
}

void grSetClearDepth(Gerbera_s *state, int depth)
{
    GR_API_CHECK(state != NULL, 455);
    GR_API_CHECK(depth >= 0, 456);
    state->clearDepth = (int)hgMulu64h(2 * depth, 0xFFFFFFFF);
}

void grSetClearStencil(Gerbera_s *state, unsigned int s)
{
    GR_API_CHECK(state != NULL, 469);
    state->clearStencil = s;
}

/* --- grClearBuffers (grApiSmall.inl 388; DLL 00443AD0) ------------------------ */

void grClearBuffers(Gerbera_s *state, unsigned int bufferMask)
{
    _grClearInfo *c;

    if (state->clearInfoIndex == GR_CLEAR_BUFFER_SIZE)
        grFlushBufferClears(state);
    GR_API_CHECK(hgInRange32(state->clearInfoIndex, 0, GR_CLEAR_BUFFER_SIZE - 1), 395);
    c = &state->clearInfos[state->clearInfoIndex++];
    GR_API_CHECK(state != NULL && bufferMask != 0, 399);
    c->bufferMask = (unsigned char)bufferMask;
    c->scissor = state->surfaceScissor;
    c->colorMask = (unsigned char)state->colorMask;
    c->depthMask = (unsigned char)state->depthMask;
    c->stencilWriteMask = (unsigned char)state->stencilWriteMask;
    c->clearColor = state->clearColor;
    c->clearDepth = state->clearDepth;
    c->clearStencil = state->clearStencil;
    c->useDithering = (unsigned char)state->useDithering;
}

/* --- cull mode ---------------------------------------------------------------- */

void grSetCullMode(Gerbera_s *state, int frontFace, unsigned int cullMask)
{
    GR_API_CHECK(state != NULL, 483);
    GR_API_CHECK(frontFace == GR_WINDING_CW || frontFace == GR_WINDING_CCW, 484);
    GR_API_CHECK((cullMask & 0xFFFFFFFCu) == 0, 485);
    state->frontFace = frontFace;
    state->cullMask = cullMask;
    grUpdateCullMode(state);
}

/* --- fog ----------------------------------------------------------------------- */

void grSetFogMode(Gerbera_s *state, int fogMode)
{
    GR_API_CHECK(state != NULL, 542);
    GR_API_CHECK(hgInRange32(fogMode, 0, GR_FOGMODE_MAX - 1), 543);
    state->fogMode = fogMode;
}

void grSetFogDensity(Gerbera_s *state, int density)
{
    GR_API_CHECK(state != NULL, 556);
    state->fogDensity = density;
}

void grSetFogRange(Gerbera_s *state, int fogStart, int fogEnd)
{
    int diff;

    GR_API_CHECK(state != NULL, 572);
    state->fogStart = fogStart;
    state->fogEnd = fogEnd;
    diff = IScalar_sub(state->fogEnd, state->fogStart);
    GR_API_CHECK(state != NULL, 579);
    if (IScalar_testeq(diff) != 0)
    {
        state->fogOORange = IScalar_zero();
        state->fogAtZero = 0xFFFF;
    }
    else
    {
        int v;
        state->fogOORange = IScalar_rcp(diff);
        v = IScalar_mul(fogEnd, state->fogOORange);
        v = IScalar_scalarToFixed(v, 16);
        state->fogAtZero = ~(unsigned int)(unsigned short)hgClamp32_0_x(v, 0xFFFF);
    }
}

void grSetFogColor(Gerbera_s *state, unsigned int fogColor)
{
    GR_API_CHECK(state != NULL, 606);
    state->fogColor = fogColor;
    state->pixelPipelineDirty = 1;
}

/* --- gradients ------------------------------------------------------------------ */

void grSetRectGradients(Gerbera_s *state, int gradients)
{
    GR_API_CHECK(state != NULL, 622);
    GR_API_CHECK(grIsValidBool(gradients) != 0, 623);
    if (state->screenRectGradients != gradients)
    {
        state->screenRectGradients = gradients;
        state->pixelPipelineDirty = 1;
    }
}

/* --- global ambient --------------------------------------------------------------- */

void grSetGlobalAmbient(Gerbera_s *state, _XColor3 *ambient)
{
    GR_API_CHECK(state != NULL, 764);
    if (ambient->r != state->ambientColor.r ||
        ambient->g != state->ambientColor.g ||
        ambient->b != state->ambientColor.b)
    {
        state->ambientColor = *ambient;
        state->ambientLightSumDirty = 1;
    }
}

/* --- material ------------------------------------------------------------------------ */

void grSetMaterialColor(Gerbera_s *state, int component, _XColor3 *color)
{
    GR_API_CHECK(state != NULL, 658);
    GR_API_CHECK(hgInRange32(component, 0, GR_MATERIALCOLOR_MAX - 1), 659);
    if (color->r != state->material.colors[component].r ||
        color->g != state->material.colors[component].g ||
        color->b != state->material.colors[component].b)
    {
        state->material.colors[component] = *color;
        if (component == GR_MATERIALCOLOR_EMISSION)
            state->baseColorDirty = 1;
        else
        {
            state->lightColorDirtyMask = 0xFF;
            if (component == GR_MATERIALCOLOR_AMBIENT)
                state->baseColorDirty = 1;
        }
    }
}

void grSetMaterialAlpha(Gerbera_s *state, unsigned short alpha)
{
    GR_API_CHECK(state != NULL, 708);
    GR_API_CHECK(hgInRange32(alpha, 0, 0xFFFF), 709);
    state->material.alpha = alpha;
}

void grSetMaterialShininess(Gerbera_s *state, int shininess)
{
    GR_API_CHECK(state != NULL, 740);
    GR_API_CHECK(IScalar_testge(shininess) != 0, 742);
    state->material.shininess = IScalar_toXScalar(shininess);
}

int grGetMaterialShininess(Gerbera_s *state)
{
    GR_API_CHECK(state != NULL, 752);
    return XScalar_toIScalar(state->material.shininess);
}

/* --- lights --------------------------------------------------------------------------- */

void grSetLightPosition(Gerbera_s *state, int ndx, _IFVector4 *position)
{
    _grLight *light;

    GR_API_CHECK(state != NULL, 788);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 789);
    light = &state->lights[ndx];
    light->positional = IFVector4_tov3(light->positionXYZ, position);
    if (light->positional == 0)
        XNormal3_normalize(light->nVP, light->positionXYZ[0].m);
    IFVector4_copy(&light->orgPosition, position);
    state->lightTypeDirtyMask |= 1u << ndx;
}

_IFVector4 *grGetLightOrgPosition(Gerbera_s *state, int ndx)
{
    GR_API_CHECK(state != NULL, 815);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 816);
    return &state->lights[ndx].orgPosition;
}

void grSetLightSpotDirection(Gerbera_s *state, int ndx, int *direction, _IFVector3 *orgDirection)
{
    GR_API_CHECK(state != NULL, 826);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 827);
    XNormal3_negate(state->lights[ndx].spotDirectionNegated, direction);
    IFVector3_copy(&state->lights[ndx].orgSpotDirection, orgDirection);
    state->lightTypeDirtyMask |= 1u << ndx;
}

_IFVector3 *grGetLightOrgDirection(Gerbera_s *state, int ndx)
{
    GR_API_CHECK(state != NULL, 845);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 846);
    return &state->lights[ndx].orgSpotDirection;
}

void grSetLightColor(Gerbera_s *state, int ndx, int component, _XColor3 *color)
{
    GR_API_CHECK(state != NULL, 856);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 857);
    GR_API_CHECK(hgInRange32(component, 0, 2), 858);
    state->lights[ndx].colors[component] = *color;
    state->lightColorDirtyMask |= 1u << ndx;
}

void grGetLightColor(Gerbera_s *state, int ndx, int component, _XColor3 *color)
{
    GR_API_CHECK(state != NULL, 870);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 871);
    GR_API_CHECK(hgInRange32(component, 0, 2), 872);
    *color = state->lights[ndx].colors[component];
}

void grSetLightAttenuation(Gerbera_s *state, int ndx, int component, int value)
{
    GR_API_CHECK(state != NULL, 882);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 883);
    GR_API_CHECK(hgInRange32(component, 0, 2), 884);
    GR_API_CHECK(IScalar_testge(value) != 0, 885);
    IFVector3_setf(&state->lights[ndx].attenuation, component, value);
    state->lightTypeDirtyMask |= 1u << ndx;
}

int grGetLightAttenuation(Gerbera_s *state, int ndx, int component)
{
    GR_API_CHECK(state != NULL, 897);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 898);
    GR_API_CHECK(hgInRange32(component, 0, 2), 899);
    return IFVector3_getf(&state->lights[ndx].attenuation, component);
}

void grSetLightExponent(Gerbera_s *state, int ndx, int exponent)
{
    GR_API_CHECK(state != NULL, 914);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 915);
    state->lights[ndx].exponent = IScalar_toXScalar(exponent);
    state->lightTypeDirtyMask |= 1u << ndx;
}

int grGetLightExponent(Gerbera_s *state, int ndx)
{
    GR_API_CHECK(state != NULL, 927);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 928);
    return XScalar_toIScalar(state->lights[ndx].exponent);
}

void grSetLightSpotCutoff(Gerbera_s *state, int ndx, int cutoff)
{
    int cos, zero;

    GR_API_CHECK(state != NULL, 938);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 939);
    state->lights[ndx].spotCutoff = cutoff;
    zero = IScalar_zero();
    cos = IScalar_cos(IScalar_degreesToRadians(cutoff));
    state->lights[ndx].cosSpotCutoff = IScalar_toXScalar(IScalar_max(cos, zero));
    state->lightTypeDirtyMask |= 1u << ndx;
}

int grGetLightSpotCutoff(Gerbera_s *state, int ndx)
{
    GR_API_CHECK(state != NULL, 952);
    GR_API_CHECK(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1), 953);
    return state->lights[ndx].spotCutoff;
}

/* --- alpha / stencil / depth / blend / masks ---------------------------------------- */

void grSetAlphaFunc(Gerbera_s *state, int func, int ref)
{
    GR_API_CHECK(state != NULL, 966);
    GR_API_CHECK(hgInRange32(func, 0, GR_FUNC_MAX - 1), 967);
    GR_API_CHECK(IScalar_testge(ref) != 0 && IScalar_cmple(ref, IScalar_one()) != 0, 968);
    if (func != state->alphaFunc)
    {
        state->pixelPipelineDirty = 1;
        state->rejectAllStateDirty = 1;
    }
    state->alphaFunc = func;
    state->alphaReference = hgClamp32_0_x(IScalar_scalarToFixed(ref, 8), 255);
}

void grSetStencilFunc(Gerbera_s *state, int func, unsigned int ref, unsigned int mask)
{
    GR_API_CHECK(state != NULL, 984);
    GR_API_CHECK(hgInRange32(func, 0, GR_FUNC_MAX - 1), 985);
    if (func != state->stencilFunc || mask != state->stencilTestMask)
    {
        state->stencilFunc = func;
        state->stencilTestMask = mask;
        state->pixelPipelineDirty = 1;
        state->rejectAllStateDirty = 1;
    }
    state->stencilReference = ref;
}

void grGetDrawBufferDescriptor(Gerbera_s *state, _grSurfaceDescriptor *desc)
{
    GR_API_CHECK(state != NULL && desc != NULL, 1008);
    /* whole-int copies across the packed descriptor prefix, like the decompile */
    *(int *)desc->rgbaOffsets = *(int *)state->drawSurface.rgbaOffsets;
    *(int *)desc->rgbaSizes = *(int *)state->drawSurface.rgbaSizes;
    *(int *)&desc->colorMask = *(int *)&state->drawSurface.colorMask;
    *(int *)&desc->pixelFormat = *(int *)&state->drawSurface.pixelFormat;
}

void grSetDepthOffset(Gerbera_s *state, int factor, int units)
{
    GR_API_CHECK(state != NULL, 1014);
    if (hgSign32(state->depthOffsetFactor) != hgSign32(factor) ||
        hgSign32(state->depthOffsetUnits) != hgSign32(units))
    {
        state->pixelPipelineDirty = 1;
    }
    state->depthOffsetFactor = factor;
    state->depthOffsetUnits = units;
}

void grSetStencilOp(Gerbera_s *state, int sFail, int dFail, int dPass)
{
    GR_API_CHECK(state != NULL, 1025);
    GR_API_CHECK(hgInRange32(sFail, 0, GR_STENCILOP_MAX - 1), 1026);
    GR_API_CHECK(hgInRange32(dFail, 0, GR_STENCILOP_MAX - 1), 1027);
    GR_API_CHECK(hgInRange32(dPass, 0, GR_STENCILOP_MAX - 1), 1028);
    if (state->stencilFail != sFail || state->stencilZFail != dFail || state->stencilZPass != dPass)
    {
        state->pixelPipelineDirty = 1;
        state->stencilFail = sFail;
        state->stencilZFail = dFail;
        state->stencilZPass = dPass;
    }
}

void grSetDepthFunc(Gerbera_s *state, int func)
{
    GR_API_CHECK(state != NULL, 1042);
    GR_API_CHECK(hgInRange32(func, 0, GR_FUNC_MAX - 1), 1043);
    if (state->depthFunc != func)
    {
        state->depthFunc = func;
        state->pixelPipelineDirty = 1;
    }
}

void grSetLogicOp(Gerbera_s *state, int logicOp)
{
    GR_API_CHECK(state != NULL, 1054);
    GR_API_CHECK(hgInRange32(logicOp, 0, GR_LOGICOP_MAX - 1), 1055);
    if (state->logicOp != logicOp)
    {
        state->logicOp = logicOp;
        state->pixelPipelineDirty = 1;
    }
}

void grSetBlendFunc(Gerbera_s *state, int srcFactor, int dstFactor)
{
    GR_API_CHECK(state != NULL, 1079);
    GR_API_CHECK(hgInRange32(srcFactor, 0, GR_BLENDFACTOR_MAX - 1), 1080);
    GR_API_CHECK(hgInRange32(dstFactor, 0, GR_BLENDFACTOR_MAX - 1), 1081);
    if (state->srcBlendFactor != srcFactor || state->dstBlendFactor != dstFactor)
    {
        state->srcBlendFactor = srcFactor;
        state->dstBlendFactor = dstFactor;
        state->pixelPipelineDirty = 1;
    }
}

void grSetColorMask(Gerbera_s *state, unsigned int mask)
{
    GR_API_CHECK(state != NULL, 1093);
    GR_API_CHECK((mask & 0xFFFFFFF0u) == 0, 1094);
    if (state->colorMask != mask)
    {
        state->colorMask = mask;
        state->pixelPipelineDirty = 1;
    }
}

void grSetDepthMask(Gerbera_s *state, int mask)
{
    GR_API_CHECK(state != NULL, 1104);
    GR_API_CHECK(grIsValidBool(mask) != 0, 1105);
    if (state->depthMask != mask)
    {
        state->depthMask = mask;
        state->pixelPipelineDirty = 1;
    }
}

void grSetStencilMask(Gerbera_s *state, unsigned int mask)
{
    GR_API_CHECK(state != NULL, 1115);
    if (state->stencilWriteMask != mask)
    {
        state->stencilWriteMask = mask;
        state->pixelPipelineDirty = 1;
    }
}

/* --- transform stack (decompile 41689-41713) ------------------------------- */

void grSetMatrix(Gerbera_s *state, int mode, _IMatrix4x4 *matrix)
{
    GR_API_CHECK(state != NULL, 1743);
    GR_API_CHECK(matrix != NULL, 1744);
    GR_API_CHECK(hgInRange32(mode, 0, GR_MATRIXMODE_MAX - 1) != 0, 1745);
    IMatrix4x4_copy(&state->matrix[mode], matrix);
    grSetMatrixDirty(state, mode, 1);
}

/* --- user clip planes (decompile 39699-39745) -------------------------------
 * The decompile's storage expressions are Hex-Rays type artifacts:
 *   equation:  IFVector4_copy(&state->normalsToEyePal[17][0].originalVertex
 *              + plane, equation)  ==  EPOC state+4648+20*plane
 *              == origUserClipPlanes[plane-GR_CLIP_USER0] (base +4848)
 *   matrix:    IMatrix4x4_copy(&state->texCoordTransform[8][0]
 *              .padding[80*plane - 1], modelview)  ==  EPOC
 *              state+4168+80*plane == userClipMatrix
 *              [plane-GR_CLIP_USER0] (base +4968).                        */

void grSetUserClipPlane(Gerbera_s *state, int plane, _IFVector4 *equation, _IMatrix4x4 *modelview)
{
    GR_API_CHECK(state != NULL, 503);
    GR_API_CHECK(hgInRange32(plane, GR_CLIP_USER0, GR_CLIP_USER0 + GR_MAX_USER_CLIP_PLANES - 1) != 0, 504);
    IFVector4_copy(&state->origUserClipPlanes[plane - GR_CLIP_USER0], equation);
    IMatrix4x4_copy(&state->userClipMatrix[plane - GR_CLIP_USER0], modelview);
}

_IFVector4 *grGetUserClipPlane(Gerbera_s *state, int plane)
{
    GR_API_CHECK(state != NULL, 521);
    GR_API_CHECK(hgInRange32(plane, GR_CLIP_USER0, GR_CLIP_USER0 + GR_MAX_USER_CLIP_PLANES - 1) != 0, 522);
    return &state->origUserClipPlanes[plane - GR_CLIP_USER0];
}

/* --- vertex array record access (decompile 41632-41687) --------------------- */

void *grGetVertexArrayPointer(Gerbera_s *state, int arrayType)
{
    GR_API_CHECK(state != NULL, 1709);
    GR_API_CHECK(hgInRange32(arrayType, 0, 8 - 1) != 0, 1710);
    return state->vertexArray[arrayType].origPointer;
}

unsigned int grGetVertexArrayBinding(Gerbera_s *state, int arrayType)
{
    GR_API_CHECK(state != NULL, 1719);
    GR_API_CHECK(hgInRange32(arrayType, 0, 8 - 1) != 0, 1720);
    return state->vertexArray[arrayType].binding;
}

void grResetVertexArray(Gerbera_s *state, int arrayType)
{
    GR_API_CHECK(state != NULL, 1729);
    GR_API_CHECK(hgInRange32(arrayType, 0, 8 - 1) != 0, 1730);
    state->vertexArray[arrayType].data = NULL;
}
