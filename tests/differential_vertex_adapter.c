/* Host-pointer fixture for complete vertex-buffer stage comparisons. */
#include <stdint.h>
#include <string.h>
#include "gerbera/grState.h"

typedef char vertex_record_size[(sizeof(_grVertex) == 128) ? 1 : -1];
typedef char texture_transform_size[(sizeof(_ITCTransform) == 132) ? 1 : -1];

enum DifferentialVertexStage
{
    DIFF_NORMAL_BYTE, DIFF_NORMAL_SHORT, DIFF_NORMAL_FIXED, DIFF_NORMAL_FLOAT,
    DIFF_NORMAL_DEFAULT,
    DIFF_COLOR_BYTE, DIFF_COLOR_FLOAT, DIFF_COLOR_FIXED, DIFF_COLOR_DEFAULT,
    DIFF_COLOR_FAST_BYTE, DIFF_COLOR_FAST_FLOAT, DIFF_COLOR_FAST_FIXED,
    DIFF_COLOR_FAST_DEFAULT,
    DIFF_FOG_FAST, DIFF_FOG_LINEAR, DIFF_FOG_EXP, DIFF_FOG_EXP2,
    DIFF_POINT_FLOAT, DIFF_POINT_FIXED,
    DIFF_TRANSFORM_VERTEX, DIFF_VIEWPORT_VERTEX, DIFF_CACHE_VERTEX
};

void differential_vertex_stage(unsigned int operation, const int32_t control[16],
                               void *array_data, void *render_stack,
                               unsigned char *vertex_list,
                               const _IVertexTransform *transform)
{
    static void (*const stages[])(Gerbera_s *, unsigned char *) = {
        grFetchVertexNormalINT8, grFetchVertexNormalINT16,
        grFetchVertexNormalFIXED, grFetchVertexNormalFLOAT,
        grFetchVertexNormalDEFAULT,
        grFetchVertexColorUINT8, grFetchVertexColorFLOAT,
        grFetchVertexColorFIXED, grFetchVertexColorDEFAULT,
        grFetchVertexColorFastUINT8, grFetchVertexColorFastFLOAT,
        grFetchVertexColorFastFIXED, grFetchVertexColorFastDEFAULT,
        grApplyFogFAST, grApplyFogLINEAR, grApplyFogEXP, grApplyFogEXP2,
        grFetchPointSizeFLOAT, grFetchPointSizeFIXED
    };
    Gerbera_s state;
    unsigned int i;
    memset(&state, 0, sizeof(state));
    state.renderStack = render_stack;
    state.vertexCache = render_stack;
    for (i = 0; i < 8; ++i)
    {
        state.vertexArray[i].stride = control[0];
        state.vertexArray[i].size = (unsigned char)control[1];
        state.vertexArray[i].type = (unsigned char)control[2];
        state.vertexArray[i].data = array_data;
    }
    memcpy(state.defaultNormal, control + 3, sizeof(state.defaultNormal));
    memcpy(&state.defaultColor, control + 6, sizeof(state.defaultColor));
    for (i = 0; i < 4; ++i)
        state.defaultColorRGBA[i] = (unsigned short)control[9 + i];
    state.fogDensity = control[13];
    state.fogEnd = control[14];
    state.fogOORange = control[15];
    state.modelviewTransform = *transform;
    state.objectToClipTransform = *transform;
    state.projectionTransform = *transform;
    state.viewportScale.normal[0] = 320 << 8;
    state.viewportScale.normal[1] = -(240 << 8);
    state.viewportScale.normal[2] = 0x1FFFFFFF;
    state.viewportOffset.normal[0] = 320 << 8;
    state.viewportOffset.normal[1] = 240 << 8;
    state.viewportOffset.normal[2] = 0x1FFFFFFF;
    if (operation < sizeof(stages) / sizeof(stages[0]))
        stages[operation](&state, vertex_list);
    else
    {
        _grVertex *vertices = (_grVertex *)((unsigned char *)render_stack + 1332);
        for (; *vertex_list != 255; ++vertex_list)
        {
            if (operation == DIFF_TRANSFORM_VERTEX)
                grTransformVertex(&state, &vertices[*vertex_list]);
            else if (operation == DIFF_VIEWPORT_VERTEX)
                grViewportTransformVertex(&state, &vertices[*vertex_list]);
            else if (operation == DIFF_CACHE_VERTEX)
                grVertexCacheGet(&state, *vertex_list);
        }
    }
}

void differential_vertex_lighting(const int32_t control[7], void *render_stack,
                                  unsigned char *vertex_list,
                                  const _IVertexTransform *transform,
                                  const _XNormalTransform *normal_transform,
                                  _grLight *lights)
{
    Gerbera_s state;
    int i;
    memset(&state, 0, sizeof(state));
    state.renderStack = render_stack;
    state.lightingSideMask = (unsigned char)control[0];
    state.capabilities[GR_CAPABILITY_NORMALIZE] = (unsigned char)control[1];
    state.capabilities[GR_CAPABILITY_COLOR_MATERIAL] = (unsigned char)control[2];
    state.material.shininess = control[3];
    state.material.alpha = 0xABCD;
    state.material.colors[3].r = 0x10000;
    state.material.colors[3].g = 0x20000;
    state.material.colors[3].b = 0x30000;
    state.ambientLightSum.r = 0x40000;
    state.ambientLightSum.g = 0x80000;
    state.ambientLightSum.b = 0xC0000;
    state.baseColor = state.ambientLightSum;
    state.normalsToEyeTransform = *normal_transform;
    state.modelviewTransform = *transform;
    state.transformVE = control[5] != 0;
    for (i = 0; i < control[4]; ++i)
        state.l_directional[i] = &lights[i];
    for (i = 0; i < control[5]; ++i)
        state.l_positional[i] = &lights[control[4] + i];
    if (control[6])
        grApplyLighting(&state, vertex_list);
    else
    {
        _grVertex *vertices = (_grVertex *)((unsigned char *)render_stack + 1332);
        for (; *vertex_list != 255; ++vertex_list)
            grLightVertex(&state, &vertices[*vertex_list]);
    }
}

void differential_vertex_texture(unsigned int operation, unsigned int unit,
                                 int size, int stride, void *array_data,
                                 void *render_stack, unsigned char *vertex_list,
                                 const _ITCTransform *transform)
{
    static void (*const stages[])(Gerbera_s *, unsigned char *, int) = {
        grTransformTexCoordINT8x2, grTransformTexCoordINT8x4,
        grTransformTexCoordINT16x2, grTransformTexCoordINT16x4,
        grTransformTexCoordFIXEDx4, grTransformTexCoordDEFAULT,
        grTransformTexCoordFLOATx4
    };
    Gerbera_s state;
    memset(&state, 0, sizeof(state));
    state.renderStack = render_stack;
    state.vertexArray[6 + unit].data = array_data;
    state.vertexArray[6 + unit].size = (unsigned char)size;
    state.vertexArray[6 + unit].stride = stride;
    state.texCoordTransform[unit] = *transform;
    state.defaultTexCoord.x[2][0] = -0x8000;
    state.defaultTexCoord.x[2][1] = 0x10000;
    state.defaultTexCoord.x[2][2] = 0x20000;
    state.defaultTexCoord.x[2][3] = -0x4000;
    stages[operation](&state, vertex_list, (int)unit);
}
