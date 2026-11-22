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
