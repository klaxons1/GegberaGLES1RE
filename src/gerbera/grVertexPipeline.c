/* grVertexPipeline.c - vertex pipeline (port of the decompiled
 * grVertexPipeline.inl; grows as the pipeline is reconstructed).
 * Assert sites keep the original grVertexPipeline.inl file/line strings. */
#include "gerbera/grState.h"

#define GR_VP_CHECK(cond, line)                  \
    do                                           \
    {                                            \
        if (!(cond) && grAssert(#cond, "grVertexPipeline.inl", (line)) != 0) \
            grDie(0);                            \
    } while (0)

unsigned int grTestLightColor(_grLight *light, int component)
{
    GR_VP_CHECK(light != NULL, 393);
    if (XColor3_isZero(&light->colors[component]) != 0)
        return 0;
    return 0xFFFFFFFFu;
}

unsigned int grPrecalcLightColor(_grLight *light, int component, _XColor3 *materialColor)
{
    GR_VP_CHECK(light != NULL && materialColor != NULL, 384);
    XColor3_mul(&light->premul_colors[component], &light->colors[component], materialColor);
    if (XColor3_isZero(&light->premul_colors[component]) != 0)
        return 0;
    return 0xFFFFFFFFu;
}

/* --- grUpdateLight (decompile 28169-28201; fragment assert line 410) -------
 * Recomputes one light's lightMask and, for directional lights, the
 * headlight "halfway" vector. flt_48E050 = 180.0 (the default spot cutoff;
 * the EPOC build compares the raw dword against 0x43340000, which is
 * IScalar_immediate(180.0f) since immediates are float bit patterns). */

unsigned int grUpdateLight(Gerbera_s *state, int ndx)
{
    unsigned int lightMask = 0;
    _grLight *l;

    if (!(hgInRange32(ndx, 0, GR_MAX_LIGHTS - 1) != 0) && grAssert("hgInRange32(ndx, 0, GR_MAX_LIGHTS-1)", "grVertexPipeline.inl", 410) != 0)
        grDie(0);
    l = &state->lights[ndx];
    if (IScalar_cmpeq(l->spotCutoff, IScalar_immediate(180.0f)) == 0)
        lightMask = 16u;
    if (l->positional != 0)
    {
        if (IFVector3_is100(&l->attenuation) == 0)
            lightMask |= 0x20u;
    }
    else
    {
        lightMask |= 0x40u;
        l->halfway[0] = l->nVP[0];
        l->halfway[1] = l->nVP[1];
        l->halfway[2] = XScalar_add(l->nVP[2], 0x200000);
        XNormal3_normalize(l->halfway, l->halfway);
    }
    l->lightMask = lightMask;
    return lightMask;
}

/* --- grUpdateAmbientLightSum (decompile 28204-28220) ------------------------
 * ambientLightSum = ambientColor + ambient color of every directional
 * light (the pointer list built by grUpdateLights). */

void grUpdateAmbientLightSum(Gerbera_s *state)
{
    _grLight **pDirectional;

    state->ambientLightSum = state->ambientColor;
    for (pDirectional = state->l_directional; *pDirectional != NULL; ++pDirectional)
        XColor3_add(&state->ambientLightSum, &state->ambientLightSum, &(*pDirectional)->colors[0]);
}

/* --- grUpdateLights (decompile 28223-28551; fragment assert lines
 * 518/609/660/685/702) -------------------------------------------------------
 * Refreshes light records whose type/color changed, partitions the enabled
 * lights into l_directional / l_positional pointer lists, recomputes the
 * premultiplied light colors against the material and refreshes
 * ambientLightSum / baseColor. Returns whether a positional light is on
 * (the vertex pipeline's transformVE bit). */

int grUpdateLights(Gerbera_s *state)
{
    unsigned int activeMask;
    unsigned char typeDirty;
    unsigned int activeDirtyMask;
    unsigned int colorDirtyMask;
    unsigned int directionalMask;
    unsigned int dirty;
    unsigned int lightMask;
    int i;
    _grLight *pLight;
    _grLight **pDir;
    _grLight **pPos;
    _XColor3 c;
    _XColor3 tmp;
    int recompute;

    activeMask = state->lightActiveMask;
    typeDirty = (unsigned char)(activeMask & state->lightTypeDirtyMask);
    activeDirtyMask = state->lightActiveDirtyMask;
    colorDirtyMask = state->lightColorDirtyMask;
    directionalMask = state->lightDirectionalMask;
    if (typeDirty != 0)
    {
        activeDirtyMask |= typeDirty;
        directionalMask = 0;
        for (i = 0; i < 8; ++i)
        {
            if (((1 << i) & typeDirty) != 0)
            {
                if ((grUpdateLight(state, i) & 0x40) != 0)
                    directionalMask |= 1 << i;
                else
                    directionalMask &= ~(1u << i);
            }
        }
        state->lightTypeDirtyMask &= ~typeDirty;
        state->lightDirectionalMask = (unsigned char)directionalMask;
    }
    directionalMask &= activeMask;
    for (i = 0; i < 8; ++i)
    {
        if (grTestCapability(state, GR_CAPABILITY_LIGHT0 + i) != 0)
        {
            if (!((activeMask & (1 << i)) != 0) && grAssert("lightActiveMask & (1<<i)", "grVertexPipeline.inl", 518) != 0)
                grDie(0);
            if ((state->lights[i].lightMask & 0x40) != 0)
            {
                if (!((directionalMask & (1 << i)) != 0) && grAssert("(lightDirectionalMask & (1<<i)) != 0", "grVertexPipeline.inl", 518) != 0)
                    grDie(0);
            }
            else
            {
                if (!((directionalMask & (1 << i)) == 0) && grAssert("(lightDirectionalMask & (1<<i)) == 0", "grVertexPipeline.inl", 518) != 0)
                    grDie(0);
            }
        }
        else
        {
            if (!((activeMask & (1 << i)) == 0) && grAssert("(lightActiveMask & (1<<i)) == 0", "grVertexPipeline.inl", 518) != 0)
                grDie(0);
            if (!((directionalMask & (1 << i)) == 0) && grAssert("(lightDirectionalMask & (1<<i)) == 0", "grVertexPipeline.inl", 518) != 0)
                grDie(0);
        }
    }
    if (activeDirtyMask != 0)
    {
        pDir = state->l_directional;
        pPos = state->l_positional;
        for (i = 0; i < 8; ++i)
        {
            if (((1 << i) & activeMask) != 0)
            {
                pLight = &state->lights[i];
                if ((pLight->lightMask & 0x40) != 0)
                    *pDir++ = pLight;
                else
                    *pPos++ = pLight;
            }
        }
        *pDir = NULL;
        *pPos = NULL;
        state->lightActiveDirtyMask = 0;
    }
    dirty = activeMask & (colorDirtyMask | typeDirty);
    recompute = state->baseColorDirty;
    if (grTestCapability(state, GR_CAPABILITY_COLOR_MATERIAL) != 0)
    {
        if (dirty != 0)
        {
            for (i = 0; i < 8; ++i)
            {
                if (((1 << i) & dirty) != 0)
                {
                    pLight = &state->lights[i];
                    lightMask = pLight->lightMask & 0xFFFFFFF1u;
                    if ((lightMask & 0x40) == 0)
                        lightMask |= grTestLightColor(pLight, 0) & 2u;
                    lightMask |= grTestLightColor(pLight, 1) & 4u;
                    lightMask |= grPrecalcLightColor(pLight, 2, &state->material.colors[2]) & 8u;
                    pLight->lightMask = lightMask;
                }
            }
            state->lightColorDirtyMask &= ~(unsigned int)(unsigned char)activeMask;
        }
        for (i = 0; i < 8; ++i)
        {
            if (grGetCapability(state, GR_CAPABILITY_LIGHT0 + i) != 0)
            {
                pLight = &state->lights[i];
                lightMask = pLight->lightMask & 0xFFFFFFF1u;
                if ((lightMask & 0x40) == 0)
                    lightMask |= grTestLightColor(pLight, 0) & 2u;
                lightMask |= grTestLightColor(pLight, 1) & 4u;
                lightMask |= grPrecalcLightColor(pLight, 2, &state->material.colors[2]) & 8u;
                if (!((unsigned int)pLight->lightMask == lightMask) && grAssert("pLight->lightMask == lightMask", "grVertexPipeline.inl", 660) != 0)
                    grDie(0);
            }
        }
    }
    else
    {
        if (dirty != 0)
        {
            for (i = 0; i < 8; ++i)
            {
                if (((1 << i) & dirty) != 0)
                {
                    pLight = &state->lights[i];
                    lightMask = pLight->lightMask & 0xFFFFFFF1u;
                    if ((lightMask & 0x40) == 0)
                        lightMask |= grPrecalcLightColor(pLight, 0, &state->material.colors[0]) & 2u;
                    lightMask |= grPrecalcLightColor(pLight, 1, &state->material.colors[1]) & 4u;
                    lightMask |= grPrecalcLightColor(pLight, 2, &state->material.colors[2]) & 8u;
                    pLight->lightMask = lightMask;
                }
            }
            state->lightColorDirtyMask &= ~(unsigned int)(unsigned char)activeMask;
        }
        for (i = 0; i < 8; ++i)
        {
            if (grGetCapability(state, GR_CAPABILITY_LIGHT0 + i) != 0)
            {
                pLight = &state->lights[i];
                lightMask = pLight->lightMask & 0xFFFFFFF1u;
                if ((lightMask & 0x40) == 0)
                    lightMask |= grPrecalcLightColor(pLight, 0, &state->material.colors[0]) & 2u;
                lightMask |= grPrecalcLightColor(pLight, 1, &state->material.colors[1]) & 4u;
                lightMask |= grPrecalcLightColor(pLight, 2, &state->material.colors[2]) & 8u;
                if (!((unsigned int)pLight->lightMask == lightMask) && grAssert("pLight->lightMask == lightMask", "grVertexPipeline.inl", 609) != 0)
                    grDie(0);
            }
        }
    }
    if (state->ambientLightSumDirty != 0 || (directionalMask & colorDirtyMask) != 0 || directionalMask != state->ambientLightSumMask)
    {
        grUpdateAmbientLightSum(state);
        recompute = 1;
        state->ambientLightSumMask = (unsigned char)directionalMask;
        state->ambientLightSumDirty = 0;
    }
    c = state->ambientLightSum;
    grUpdateAmbientLightSum(state);
    if (!(c.r == state->ambientLightSum.r) && grAssert("c->r == state->ambientLightSum->r", "grVertexPipeline.inl", 685) != 0)
        grDie(0);
    if (!(c.g == state->ambientLightSum.g) && grAssert("c->g == state->ambientLightSum->g", "grVertexPipeline.inl", 685) != 0)
        grDie(0);
    if (!(c.b == state->ambientLightSum.b) && grAssert("c->b == state->ambientLightSum->b", "grVertexPipeline.inl", 685) != 0)
        grDie(0);
    if (recompute != 0)
    {
        XColor3_mul(&tmp, &state->ambientLightSum, &state->material.colors[0]);
        XColor3_add(&state->baseColor, &tmp, &state->material.colors[3]);
        state->baseColorDirty = 0;
    }
    XColor3_mul(&tmp, &state->ambientLightSum, &state->material.colors[0]);
    XColor3_add(&tmp, &tmp, &state->material.colors[3]);
    if (!(tmp.r == state->baseColor.r) && grAssert("c->r == state->baseColor->r", "grVertexPipeline.inl", 702) != 0)
        grDie(0);
    if (!(tmp.g == state->baseColor.g) && grAssert("c->g == state->baseColor->g", "grVertexPipeline.inl", 702) != 0)
        grDie(0);
    if (!(tmp.b == state->baseColor.b) && grAssert("c->b == state->baseColor->b", "grVertexPipeline.inl", 702) != 0)
        grDie(0);
    return state->l_positional[0] != NULL;
}

/* --- vertex fetch buffer ----------------------------------------------------
 * The fetch-stage vertex records live in the grFast render stack: base =
 * renderStack[166].whichPool = (char*)renderStack + 1332, one 128-byte
 * _grVertex per vertex index (<< 7 addressing). */

#define GR_RENDER_STACK_BYTE(state, offset) \
    ((unsigned char *)(state)->renderStack + (offset))
#define GR_VERTEX_BUFFER(state) \
    ((_grVertex *)GR_RENDER_STACK_BYTE(state, 1332))

#define GR_FETCH_CHECK(vertexList, line)                                 \
    do                                                                   \
    {                                                                    \
        int ok__ = 1;                                                    \
        int cond__ = ((vertexList) != NULL && *(vertexList) != 0xFF);    \
        if (!cond__ && grAssert("vertexList && (* vertexList!=(HGuint8)GR_INVALID_INDEX)", \
                                "grVertexPipeline.inl", (line)) != 0)    \
            ok__ = 0;                                                \
        if (ok__ == 0)                                                   \
            grDie(0);                                                    \
    } while (0)

/* --- grGetVertexPositionState / grSetVertexPositionState (27797/27813) ------ */

int grGetVertexPositionState(_grVertex *v)
{
    return (int)v->stateMask >> 14;
}

/* --- grGetFogZ (30145-30180; assert grVertexPipeline.inl 1911) --------------
 * The eye-space Z of a vertex depending on how far grTransformVertex got. */

int grGetFogZ(Gerbera_s *state, _grVertex *vertex)
{
    int result;

    switch (grGetVertexPositionState(vertex))
    {
        case 0:
            result = IVertexTransform_zmulfv4(&state->modelviewTransform,
                                              vertex->position.objectSpaceFloat);
            break;
        case 1:
            result = IVertexTransform_zmulxv4(&state->modelviewTransform,
                                              (int *)&vertex->position);
            break;
        case 2:
            result = IFVector4_getf(vertex->position.objectSpaceFloat, 2);
            break;
        default:
            if (grGetVertexPositionState(vertex) != 3 &&
                grAssert("grGetVertexPositionState(vertex) == GR_POSITION_EYE_SPACE_CARTESIAN",
                         "grVertexPipeline.inl", 1911) != 0)
                grDie(0);
            result = vertex->position.objectSpaceFloat[0].m[3];
            break;
    }
    return result;
}

/* --- fog (fragments 2254-2416) ----------------------------------------------
 * All four write the fog blend factor into color.final[1][3] (u16). */

void grApplyFogFAST(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1886);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        vertex->color.final[1][3] = 0xFFFFu;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grApplyFogLINEAR(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;
    int fogOORange;
    int d;

    GR_FETCH_CHECK(vertexList, 1925);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        d = IScalar_abs(grGetFogZ(state, vertex));
        fogOORange = state->fogOORange;
        vertex->color.final[1][3] =
            (unsigned short)~(unsigned short)hgClamp32_0_x(
                IScalar_scalarToFixed(IScalar_mul(IScalar_sub(state->fogEnd, d), fogOORange), 16),
                0xFFFF);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grApplyFogEXP(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;
    int d;
    int da;

    GR_FETCH_CHECK(vertexList, 1943);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        d = IScalar_abs(grGetFogZ(state, vertex));
        da = IScalar_negate(IScalar_mul(d, state->fogDensity));
        vertex->color.final[1][3] =
            XScalar_tous(XScalar_sub(0x200000, IScalar_expe(da)));
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grApplyFogEXP2(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;
    int d;
    int da;

    GR_FETCH_CHECK(vertexList, 1963);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        d = IScalar_abs(grGetFogZ(state, vertex));
        da = IScalar_negate(IScalar_sqr(IScalar_mul(d, state->fogDensity)));
        vertex->color.final[1][3] =
            XScalar_tous(XScalar_sub(0x200000, IScalar_expe(da)));
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

/* --- color fetchers (fragments 1347-1748) -----------------------------------
 * Lighting path (grFetchVertexColor*): XColor3 int view at color+0..11 and
 * the shared alpha slot final[1][2] @+12. Fast path (Fast*): u16 RGBA rows
 * final[0] (lit) and final[1] (secondary). */

void grFetchVertexColorUINT8(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *colorArray = &state->vertexArray[2];
    unsigned char *ptr = (unsigned char *)colorArray->data;
    int stride = colorArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1487);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];
        unsigned char b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3];

        XColor3_setub(&vertex->color.input.rgb[0], b0, b1, b2);
        vertex->color.final[1][2] = (unsigned short)(b3 | (b3 << 8));
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorFLOAT(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *colorArray = &state->vertexArray[2];
    unsigned char *ptr = (unsigned char *)colorArray->data;
    int stride = colorArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1514);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        int *p = (int *)&ptr[stride * vertex->index];

        if (grIsAligned(p, 4) == 0 && grAssert("grIsAligned(vptr, 4)", "grVertexPipeline.inl", 1521) != 0)
            grDie(0);
        XColor3_setf(&vertex->color.input.rgb[0], p[0], p[1], p[2]);
        vertex->color.final[1][2] =
            (unsigned short)hgClamp32_0_x(IScalar_scalarToFixed(IScalar_iftos(p[3]), 16), 0xFFFF);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorFIXED(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *colorArray = &state->vertexArray[2];
    unsigned char *ptr = (unsigned char *)colorArray->data;
    int stride = colorArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1542);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        int *vptr = (int *)&ptr[stride * vertex->index];

        if (grIsAligned(vptr, 4) == 0 && grAssert("grIsAligned(vptr, 4)", "grVertexPipeline.inl", 1545) != 0)
            grDie(0);
        XColor3_setx(&vertex->color.input.rgb[0], vptr[0], vptr[1], vptr[2]);
        vertex->color.final[1][2] = (unsigned short)hgClamp32_0_x(vptr[3], 0xFFFF);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorDEFAULT(Gerbera_s *state, unsigned char *vertexList)
{
    int r = state->defaultColor.r;
    int g = state->defaultColor.g;
    int b = state->defaultColor.b;
    unsigned short a = state->defaultColorRGBA[3];
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1566);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        vertex->color.input.rgb[0].r = r;
        vertex->color.input.rgb[0].g = g;
        vertex->color.input.rgb[0].b = b;
        vertex->color.final[1][2] = a;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorFastUINT8(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *colorArray = &state->vertexArray[2];
    unsigned char *ptr = (unsigned char *)colorArray->data;
    int stride = colorArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1336);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];
        unsigned short r = (unsigned short)(p[0] << 8);
        unsigned short g = (unsigned short)(p[1] << 8);
        unsigned short b = (unsigned short)(p[2] << 8);
        unsigned short a = (unsigned short)(p[3] << 8);

        vertex->color.final[0][0] = r;
        vertex->color.final[0][1] = g;
        vertex->color.final[0][2] = b;
        vertex->color.final[1][0] = r;
        vertex->color.final[1][1] = g;
        vertex->color.final[1][2] = b;
        vertex->color.final[0][3] = a;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorFastFLOAT(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *colorArray = &state->vertexArray[2];
    unsigned char *ptr = (unsigned char *)colorArray->data;
    int stride = colorArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1375);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        int *p = (int *)&ptr[stride * vertex->index];
        unsigned short r, g, b, a;

        if (grIsAligned(p, 4) == 0 && grAssert("grIsAligned(s, 4)", "grVertexPipeline.inl", 1379) != 0)
            grDie(0);
        r = (unsigned short)(hgClamp32_0_x(IScalar_scalarToFixed(IScalar_iftos(p[0]), 16), 65280) & 0xFFFFFF00u);
        g = (unsigned short)(hgClamp32_0_x(IScalar_scalarToFixed(IScalar_iftos(p[1]), 16), 65280) & 0xFFFFFF00u);
        b = (unsigned short)(hgClamp32_0_x(IScalar_scalarToFixed(IScalar_iftos(p[2]), 16), 65280) & 0xFFFFFF00u);
        a = (unsigned short)(hgClamp32_0_x(IScalar_scalarToFixed(IScalar_iftos(p[3]), 16), 65280) & 0xFFFFFF00u);
        vertex->color.final[0][0] = r;
        vertex->color.final[0][1] = g;
        vertex->color.final[0][2] = b;
        vertex->color.final[1][0] = r;
        vertex->color.final[1][1] = g;
        vertex->color.final[1][2] = b;
        vertex->color.final[0][3] = a;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorFastFIXED(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *colorArray = &state->vertexArray[2];
    unsigned char *ptr = (unsigned char *)colorArray->data;
    int stride = colorArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1418);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        int *p = (int *)&ptr[stride * vertex->index];
        unsigned short r, g, b, a;

        if (grIsAligned(p, 4) == 0 && grAssert("grIsAligned(s, 4)", "grVertexPipeline.inl", 1422) != 0)
            grDie(0);
        r = (unsigned short)(hgClamp32_0_x(p[0], 65280) & 0xFFFFFF00u);
        g = (unsigned short)(hgClamp32_0_x(p[1], 65280) & 0xFFFFFF00u);
        b = (unsigned short)(hgClamp32_0_x(p[2], 65280) & 0xFFFFFF00u);
        a = (unsigned short)(hgClamp32_0_x(p[3], 65280) & 0xFFFFFF00u);
        vertex->color.final[0][0] = r;
        vertex->color.final[0][1] = g;
        vertex->color.final[0][2] = b;
        vertex->color.final[1][0] = r;
        vertex->color.final[1][1] = g;
        vertex->color.final[1][2] = b;
        vertex->color.final[0][3] = a;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexColorFastDEFAULT(Gerbera_s *state, unsigned char *vertexList)
{
    unsigned short r = (unsigned short)(state->defaultColorRGBA[0] & 0xFF00u);
    unsigned short g = (unsigned short)(state->defaultColorRGBA[1] & 0xFF00u);
    unsigned short b = (unsigned short)(state->defaultColorRGBA[2] & 0xFF00u);
    unsigned short a = (unsigned short)(state->defaultColorRGBA[3] & 0xFF00u);
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1461);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        vertex->color.final[0][0] = r;
        vertex->color.final[0][1] = g;
        vertex->color.final[0][2] = b;
        vertex->color.final[1][0] = r;
        vertex->color.final[1][1] = g;
        vertex->color.final[1][2] = b;
        vertex->color.final[0][3] = a;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

/* --- normal fetchers (fragments 1749-1955) ----------------------------------
 * All write the XNormal3 int triple at vertex->normal (+92). */

void grFetchVertexNormalINT8(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *normalArray = &state->vertexArray[1];
    unsigned char *ptr = (unsigned char *)normalArray->data;
    int stride = normalArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1590);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];

        XNormal3_setb(vertex->normal, (HGint8)p[0], (HGint8)p[1], (HGint8)p[2]);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexNormalINT16(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *normalArray = &state->vertexArray[1];
    unsigned char *ptr = (unsigned char *)normalArray->data;
    int stride = normalArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1613);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        short *p = (short *)&ptr[stride * vertex->index];

        XNormal3_sets(vertex->normal, p[0], p[1], p[2]);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexNormalFIXED(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *normalArray = &state->vertexArray[1];
    unsigned char *ptr = (unsigned char *)normalArray->data;
    int stride = normalArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1636);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        int *vptr = (int *)&ptr[stride * vertex->index];

        if (grIsAligned(vptr, 4) == 0 && grAssert("grIsAligned(vptr, 4)", "grVertexPipeline.inl", 1639) != 0)
            grDie(0);
        XNormal3_setx(vertex->normal, vptr[0], vptr[1], vptr[2]);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexNormalFLOAT(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *normalArray = &state->vertexArray[1];
    unsigned char *ptr = (unsigned char *)normalArray->data;
    int stride = normalArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1661);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        int *vptr = (int *)&ptr[stride * vertex->index];

        if (grIsAligned(vptr, 4) == 0 && grAssert("grIsAligned(vptr, 4)", "grVertexPipeline.inl", 1664) != 0)
            grDie(0);
        XNormal3_setf(vertex->normal, *(HGfloat *)&vptr[0], *(HGfloat *)&vptr[1], *(HGfloat *)&vptr[2]);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchVertexNormalDEFAULT(Gerbera_s *state, unsigned char *vertexList)
{
    int x = state->defaultNormal[0];
    int y = state->defaultNormal[1];
    int z = state->defaultNormal[2];
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1684);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        vertex->normal[0] = x;
        vertex->normal[1] = y;
        vertex->normal[2] = z;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

/* --- texture coordinate transformers (fragments 1956-2253) ------------------
 * Each walks the vertex list, transforms the raw texcoord through
 * state->texCoordTransform[texUnit] and stores the 16.16 (x,y) result at
 * vertex->texCoord[texUnit] (+56 + 8*texUnit). */

void grTransformTexCoordINT8x2(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    _grVertexArray *array = &state->vertexArray[6 + texUnit];
    unsigned char *ptr = (unsigned char *)array->data;
    int stride = array->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1708);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];
        int st[2];

        st[0] = (int)(signed char)p[0] << 16;
        st[1] = (int)(signed char)p[1] << 16;
        ITCTransform_mulxv(&vertex->texCoord[texUnit].x,
                           &state->texCoordTransform[texUnit], st, 2);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grTransformTexCoordINT8x4(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    _grVertexArray *array = &state->vertexArray[6 + texUnit];
    unsigned char *ptr = (unsigned char *)array->data;
    int stride = array->stride;
    int size = array->size;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1736);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];
        int strq[4];

        strq[0] = (int)(signed char)p[0] << 16;
        strq[1] = (int)(signed char)p[1] << 16;
        strq[2] = (int)(signed char)p[2] << 16;
        if (size == 4)
            strq[3] = (int)(signed char)p[3] << 16;
        ITCTransform_mulxv(&vertex->texCoord[texUnit].x,
                           &state->texCoordTransform[texUnit], strq, size);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grTransformTexCoordINT16x2(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    _grVertexArray *array = &state->vertexArray[6 + texUnit];
    unsigned char *ptr = (unsigned char *)array->data;
    int stride = array->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1766);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];
        int st[2];

        st[0] = (int)*(short *)(p + 0) << 16;
        st[1] = (int)*(short *)(p + 2) << 16;
        ITCTransform_mulxv(&vertex->texCoord[texUnit].x,
                           &state->texCoordTransform[texUnit], st, 2);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grTransformTexCoordINT16x4(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    _grVertexArray *array = &state->vertexArray[6 + texUnit];
    unsigned char *ptr = (unsigned char *)array->data;
    int stride = array->stride;
    int size = array->size;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1793);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        unsigned char *p = &ptr[stride * vertex->index];
        int strq[4];

        strq[0] = (int)*(short *)(p + 0) << 16;
        strq[1] = (int)*(short *)(p + 2) << 16;
        strq[2] = (int)*(short *)(p + 4) << 16;
        if (size == 4)
            strq[3] = (int)*(short *)(p + 6) << 16;
        ITCTransform_mulxv(&vertex->texCoord[texUnit].x,
                           &state->texCoordTransform[texUnit], strq, size);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grTransformTexCoordFIXEDx4(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    _grVertexArray *array = &state->vertexArray[6 + texUnit];
    unsigned char *ptr = (unsigned char *)array->data;
    int stride = array->stride;
    int size = array->size;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1823);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        ITCTransform_mulxv(&vertex->texCoord[texUnit].x,
                           &state->texCoordTransform[texUnit],
                           (int *)&ptr[stride * vertex->index], size);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grTransformTexCoordDEFAULT(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    int tcX = state->defaultTexCoord.x[2][2 * texUnit];
    int tcY = state->defaultTexCoord.x[2][2 * texUnit + 1];
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1843);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        vertex->texCoord[texUnit].x = tcX;
        vertex->texCoord[texUnit].y = tcY;
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grTransformTexCoordFLOATx4(Gerbera_s *state, unsigned char *vertexList, int texUnit)
{
    _grVertexArray *array = &state->vertexArray[6 + texUnit];
    unsigned char *ptr = (unsigned char *)array->data;
    int stride = array->stride;
    int size = array->size;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1866);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        if (grIsAligned(&ptr[stride * vertex->index], 4) == 0 &&
            grAssert("grIsAligned(ptr + stride*vertex->index, 4)", "grVertexPipeline.inl", 1868) != 0)
            grDie(0);
        ITCTransform_mulfv(&vertex->texCoord[texUnit].x,
                           &state->texCoordTransform[texUnit],
                           (int *)&ptr[stride * vertex->index], size);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

/* --- point size fetchers (fragments 2416-2489) ------------------------------ */

void grFetchPointSizeFLOAT(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *sizeArray = &state->vertexArray[5];
    unsigned char *ptr = (unsigned char *)sizeArray->data;
    int stride = sizeArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 1990);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        float *p = (float *)&ptr[stride * vertex->index];
        int s = IScalar_scalarToFixed(IScalar_ftos(*p), 16);

        vertex->pointSize = hgClamp32(s, 0x10000, 0x400000);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

void grFetchPointSizeFIXED(Gerbera_s *state, unsigned char *vertexList)
{
    _grVertexArray *sizeArray = &state->vertexArray[5];
    unsigned char *base = (unsigned char *)sizeArray->data;
    int stride = sizeArray->stride;
    _grVertex *vertexBuffer;
    _grVertex *vertex;
    unsigned char *vertexLista;

    GR_FETCH_CHECK(vertexList, 2013);
    vertexBuffer = GR_VERTEX_BUFFER(state);
    vertexLista = vertexList + 1;
    vertex = &vertexBuffer[*vertexList];
    do
    {
        vertex->pointSize = hgClamp32(*(int *)&base[stride * vertex->index], 0x10000, 0x400000);
        vertex = &vertexBuffer[*vertexLista];
    } while (*vertexLista++ != 0xFF);
}

/* --- grUpdateVertexPipeline (decompile 30378-30651; fragment assert lines
 * 2122/2149/2170/2194/2237/2268/2294) ----------------------------------------
 * Refreshes everything the fetch stages consume: texture transforms +
 * default texcoords, light records, normal transforms, and the stage
 * function pointers (color/normal/texcoord/point-size fetch, fog). */

void grUpdateVertexPipeline(Gerbera_s *state)
{
    int transformVE;
    int computePointDistAttn;
    int u;
    int i;

    if (state->primitiveType == GR_PRIMITIVETYPE_TRIANGLE)
    {
        computePointDistAttn = (IFVector3_is100(&state->pointDistAttn) == 0);
        state->vertexPipelineDirty = (computePointDistAttn != state->computePointDistAttn);
        state->computePointDistAttn = (unsigned char)computePointDistAttn;
    }
    for (u = 1; u >= 0; --u)
    {
        _grVertexArray *array = &state->vertexArray[6 + u];

        if (grIsMatrixDirty(state, GR_MATRIXMODE_TEXTURE0 + u) != 0)
        {
            grSetMatrixDirty(state, GR_MATRIXMODE_TEXTURE0 + u, 0);
            ITCTransform_copy(&state->texCoordTransform[u],
                              &state->matrix[GR_MATRIXMODE_TEXTURE0 + u]);
        }
        if (state->textureUnitEnabled[u] != 0 && array->data == NULL)
            ITCTransform_mulfv(&state->defaultTexCoord.x[2][2 * u],
                               &state->texCoordTransform[u],
                               (int *)state->defaultTexCoord.f[u], 4);
    }
    transformVE = state->computePointDistAttn;
    if (grTestCapability(state, GR_CAPABILITY_LIGHTING) != 0)
    {
        int rescale = grTestCapability(state, GR_CAPABILITY_RESCALE_NORMAL);

        transformVE = grUpdateLights(state);
        if (state->normalsToEyeDirty != 0)
        {
            IMatrix4x4_inversetranspose(&state->normalsToEyeTransform,
                                        &state->matrix[GR_MATRIXMODE_MODELVIEW], rescale);
            state->normalsToEyeDirty = 0;
        }
        if (grTestCapability(state, GR_CAPABILITY_MATRIX_PALETTE) != 0)
        {
            for (i = 8; i >= 0; --i)
            {
                if (state->normalsToEyePalDirty[i] != 0)
                {
                    IMatrix4x4_inversetranspose(&state->normalsToEyePal[i],
                                                &state->matrix[GR_MATRIXMODE_MATRIXPALETTE0 + i], rescale);
                    state->normalsToEyePalDirty[i] = 0;
                }
            }
        }
    }
    if (state->vertexPipelineDirty == 0)
    {
        if (!(state->transformVE == transformVE) && grAssert("state->transformVE == transformVE", "grVertexPipeline.inl", 2122) != 0)
            grDie(0);
    }
    state->transformVE = (unsigned char)transformVE;

    /* color fetch stage */
    if (grTestCapability(state, GR_CAPABILITY_LIGHTING) != 0)
    {
        if (grTestCapability(state, GR_CAPABILITY_COLOR_MATERIAL) != 0)
        {
            if (grTestCapability(state, GR_CAPABILITY_COLOR_ARRAY) != 0 &&
                state->vertexArray[2].data != NULL)
            {
                switch (state->vertexArray[2].type)
                {
                    case GR_DATATYPE_UINT8:
                        state->fetchVertexColor = grFetchVertexColorUINT8;
                        break;
                    case GR_DATATYPE_FIXED:
                        state->fetchVertexColor = grFetchVertexColorFIXED;
                        break;
                    default:
                        if (state->vertexArray[2].type != GR_DATATYPE_FLOAT &&
                            grAssert("color_array->type == GR_DATATYPE_FLOAT", "grVertexPipeline.inl", 2149) != 0)
                            grDie(0);
                        state->fetchVertexColor = grFetchVertexColorFLOAT;
                        break;
                }
            }
            else
            {
                state->fetchVertexColor = grFetchVertexColorDEFAULT;
            }
        }
        else
        {
            state->fetchVertexColor = NULL;
        }
    }
    else if (grTestCapability(state, GR_CAPABILITY_COLOR_ARRAY) != 0 &&
             state->vertexArray[2].data != NULL)
    {
        switch (state->vertexArray[2].type)
        {
            case GR_DATATYPE_UINT8:
                state->fetchVertexColor = grFetchVertexColorFastUINT8;
                break;
            case GR_DATATYPE_FIXED:
                state->fetchVertexColor = grFetchVertexColorFastFIXED;
                break;
            default:
                if (state->vertexArray[2].type != GR_DATATYPE_FLOAT &&
                    grAssert("color_array->type == GR_DATATYPE_FLOAT", "grVertexPipeline.inl", 2170) != 0)
                    grDie(0);
                state->fetchVertexColor = grFetchVertexColorFastFLOAT;
                break;
        }
    }
    else
    {
        state->fetchVertexColor = grFetchVertexColorFastDEFAULT;
    }

    /* normal fetch stage */
    if (grTestCapability(state, GR_CAPABILITY_LIGHTING) != 0)
    {
        if (state->vertexArray[1].data != NULL)
        {
            switch (state->vertexArray[1].type)
            {
                case GR_DATATYPE_INT8:
                    state->fetchVertexNormal = grFetchVertexNormalINT8;
                    break;
                case GR_DATATYPE_INT16:
                    state->fetchVertexNormal = grFetchVertexNormalINT16;
                    break;
                case GR_DATATYPE_FIXED:
                    state->fetchVertexNormal = grFetchVertexNormalFIXED;
                    break;
                default:
                    if (state->vertexArray[1].type != GR_DATATYPE_FLOAT &&
                        grAssert("normal_array->type == GR_DATATYPE_FLOAT", "grVertexPipeline.inl", 2194) != 0)
                        grDie(0);
                    state->fetchVertexNormal = grFetchVertexNormalFLOAT;
                    break;
            }
        }
        else
        {
            state->fetchVertexNormal = grFetchVertexNormalDEFAULT;
        }
    }
    else
    {
        state->fetchVertexNormal = NULL;
    }

    /* texcoord fetch stages */
    for (u = 0; u < 2; ++u)
    {
        _grVertexArray *array = &state->vertexArray[6 + u];

        if (state->textureUnitEnabled[u] != 0)
        {
            if (array->data != NULL &&
                grTestCapability(state, GR_CAPABILITY_TEXTURE_COORD_ARRAY_0 + u) != 0)
            {
                switch (array->type)
                {
                    case GR_DATATYPE_INT16:
                        if (array->size == 2)
                            state->fetchTexCoord[u] = grTransformTexCoordINT16x2;
                        else
                            state->fetchTexCoord[u] = grTransformTexCoordINT16x4;
                        break;
                    case GR_DATATYPE_FLOAT:
                        state->fetchTexCoord[u] = grTransformTexCoordFLOATx4;
                        break;
                    case GR_DATATYPE_FIXED:
                        state->fetchTexCoord[u] = grTransformTexCoordFIXEDx4;
                        break;
                    default:
                        if (array->type != GR_DATATYPE_INT8 &&
                            grAssert("type == GR_DATATYPE_INT8", "grVertexPipeline.inl", 2237) != 0)
                            grDie(0);
                        if (array->size == 2)
                            state->fetchTexCoord[u] = grTransformTexCoordINT8x2;
                        else
                            state->fetchTexCoord[u] = grTransformTexCoordINT8x4;
                        break;
                }
            }
            else
            {
                state->fetchTexCoord[u] = grTransformTexCoordDEFAULT;
            }
        }
        else
        {
            state->fetchTexCoord[u] = NULL;
        }
    }

    /* point size fetch stage */
    if (grGetPrimitiveVertices(state->primitiveType) == GR_PRIMITIVETYPE_POINT &&
        grTestCapability(state, GR_CAPABILITY_POINT_SMOOTH) == 0 &&
        grTestCapability(state, GR_CAPABILITY_POINT_SIZE_ARRAY) != 0 &&
        state->vertexArray[5].data != NULL)
    {
        if (state->vertexArray[5].type == GR_DATATYPE_FLOAT)
        {
            state->fetchPointSize = grFetchPointSizeFLOAT;
        }
        else
        {
            if (state->vertexArray[5].type != GR_DATATYPE_FIXED &&
                grAssert("sizeArray->type == GR_DATATYPE_FIXED", "grVertexPipeline.inl", 2268) != 0)
                grDie(0);
            state->fetchPointSize = grFetchPointSizeFIXED;
        }
    }
    else
    {
        state->fetchPointSize = NULL;
    }

    /* fog stage */
    if (grTestCapability(state, GR_CAPABILITY_FOG) != 0 && (state->colorMask & 7) != 0)
    {
        if (state->fogMode != GR_FOGMODE_LINEAR)
        {
            if (state->fogMode == GR_FOGMODE_EXP)
            {
                state->applyFog = grApplyFogEXP;
            }
            else
            {
                if (state->fogMode != GR_FOGMODE_EXP2 &&
                    grAssert("state->fogMode == GR_FOGMODE_EXP2", "grVertexPipeline.inl", 2294) != 0)
                    grDie(0);
                state->applyFog = grApplyFogEXP2;
            }
        }
        else if (IScalar_testeq(state->fogOORange) != 0)
        {
            state->applyFog = grApplyFogFAST;
        }
        else
        {
            state->applyFog = grApplyFogLINEAR;
        }
    }
    else
    {
        state->applyFog = NULL;
    }
}
