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
