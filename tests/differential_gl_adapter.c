/* Pointer-free adapters for original-vs-recovered GL state comparisons. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gerbera/api_opengl/oglContext.h"
#include "gerbera/egl/eglInternal.h"

typedef char getter_values_must_match_original[(sizeof(_oglGetterValues) == 676) ? 1 : -1];

int diff_gl_getter_info(uint32_t token, uint32_t output[4])
{
    static const oglOutputFunc_f converters[] = {
        oglIntToOutput, oglUIntToOutput, oglBoolToOutput,
        oglIScalarToOutput, oglFloatToOutput, oglUnitValueToOutput
    };
    const _oglGetterInfo *info = oglFindGetterInfo(token);
    unsigned int i;

    if (info == NULL)
        return 0;
    output[0] = info->offset;
    output[1] = info->n;
    output[2] = info->stride;
    for (i = 0; i < sizeof(converters) / sizeof(converters[0]); ++i)
    {
        if (info->outputFunc == converters[i])
        {
            output[3] = i;
            return 1;
        }
    }
    return -1;
}

int diff_gl_getter_set(int kind, int offset, uint32_t token,
                       const void *input, void *values)
{
    oglContext_s context;
    memset(&context, 0, sizeof(context));
    memcpy(&context.getValues, values, sizeof(context.getValues));
    switch (kind)
    {
    case 0:
        oglSetGetterValueBool(&context, offset, token, (const int *)input);
        break;
    case 1:
        oglSetGetterValueInt(&context, offset, token, (const int *)input);
        break;
    case 2:
        oglSetGetterValueEnum(&context, offset, token, (const unsigned int *)input);
        break;
    case 3:
        oglSetGetterValueFloat(&context, offset, token, (const float *)input);
        break;
    default:
        return -1;
    }
    memcpy(values, &context.getValues, sizeof(context.getValues));
    return 0;
}

int diff_gl_getter_get(uint32_t token, int outputType, int activeTexture,
                       int clientActiveTexture, const void *values, void *output,
                       int initialError)
{
    oglContext_s context;
    memset(&context, 0, sizeof(context));
    context.error = (OGL_ERROR)initialError;
    context.activeTexture = (unsigned char)activeTexture;
    context.clientActiveTexture = (unsigned char)clientActiveTexture;
    memcpy(&context.getValues, values, sizeof(context.getValues));
    oglGetInternal(&context, token, output, (OGL_OUTPUTTYPE)outputType);
    return (int)context.error;
}

void diff_gl_public_set(int operation, const uint32_t input[4],
                        const uint32_t control[8], const void *values,
                        uint32_t output[185])
{
    oglContext_s context;
    Gerbera_s gerbera;
    eglState_s state;
    float floats[4];
    void *previous = egliGetGlobal();

    memset(&context, 0, sizeof(context));
    memset(&gerbera, 0, sizeof(gerbera));
    memset(&state, 0, sizeof(state));
    context.gerbera = &gerbera;
    context.error = (OGL_ERROR)control[0];
    context.matrixMode = (unsigned char)control[1];
    context.activePalMatrix = (unsigned char)control[2];
    context.activeTexture = (unsigned char)control[3];
    context.clientActiveTexture = (unsigned char)control[4];
    context.pixelStorePackAlignment = (unsigned char)control[5];
    context.pixelStoreUnpackAlignment = (unsigned char)control[6];
    memcpy(&context.getValues, values, sizeof(context.getValues));
    memcpy(floats, input, sizeof(floats));
    state.currentESContext = control[7] ? &context : NULL;
    egliSetGlobal(&state);
    switch (operation)
    {
    case 0: glClearColor(floats[0], floats[1], floats[2], floats[3]); break;
    case 1: glClearColorx((int)input[0], (int)input[1], (int)input[2], (int)input[3]); break;
    case 2: glClearDepthf(floats[0]); break;
    case 3: glClearDepthx((int)input[0]); break;
    case 4: glClearStencil((int)input[0]); break;
    case 5: glColorMask((GLboolean)input[0], (GLboolean)input[1],
                        (GLboolean)input[2], (GLboolean)input[3]); break;
    case 6: glDepthMask((GLboolean)input[0]); break;
    case 7: glStencilMask(input[0]); break;
    case 8: glActiveTexture(input[0]); break;
    case 9: glClientActiveTexture(input[0]); break;
    case 10: glPixelStorei(input[0], (int)input[1]); break;
    case 11: glMatrixMode(input[0]); break;
    case 12: glHint(input[0], input[1]); break;
    }
    egliSetGlobal(previous);

    output[0] = gerbera.clearColor;
    output[1] = (uint32_t)gerbera.clearDepth;
    output[2] = gerbera.clearStencil;
    output[3] = gerbera.colorMask;
    output[4] = (uint32_t)gerbera.depthMask;
    output[5] = gerbera.stencilWriteMask;
    output[6] = (uint32_t)gerbera.pixelPipelineDirty;
    output[7] = (uint32_t)context.error;
    output[8] = context.matrixMode;
    output[9] = context.activePalMatrix;
    output[10] = context.activeTexture;
    output[11] = context.clientActiveTexture;
    output[12] = context.pixelStorePackAlignment;
    output[13] = context.pixelStoreUnpackAlignment;
    memcpy(output + 14, &context.getValues, sizeof(context.getValues));
    output[183] = gerbera.capabilities[8];
    output[184] = gerbera.capabilities[17];
}

/* Persistent matrix fixtures exercise stack overflow/underflow, dirty flags,
 * texture/palette selection and error retention across public API calls.
 * All non-pointer context bytes and every stack slot are returned, including
 * inactive slots and padding. Original context ranges are 4..8, 88..116 and
 * 124..1068; pointer fields are fixture-owned and checked separately. */
enum { DIFF_MATRIX_DEPTH = 4, DIFF_STACK_BYTES = 4 + 80 * DIFF_MATRIX_DEPTH,
       DIFF_MATRIX_CONTEXT_BYTES = 980 };
enum DiffMatrixOperation {
    DIFF_MATRIX_MODE, DIFF_ACTIVE_TEXTURE, DIFF_PALETTE_MATRIX, DIFF_LOAD_PALETTE,
    DIFF_MATRIX_PUSH, DIFF_MATRIX_POP, DIFF_LOAD_IDENTITY, DIFF_LOAD_FLOAT,
    DIFF_LOAD_FIXED, DIFF_MULT_FLOAT, DIFF_MULT_FIXED, DIFF_ROTATE_FLOAT,
    DIFF_ROTATE_FIXED, DIFF_TRANSLATE_FLOAT, DIFF_TRANSLATE_FIXED,
    DIFF_SCALE_FLOAT, DIFF_SCALE_FIXED, DIFF_FRUSTUM_FLOAT, DIFF_FRUSTUM_FIXED,
    DIFF_ORTHO_FLOAT, DIFF_ORTHO_FIXED, DIFF_GET_ERROR
};
typedef struct DiffMatrixSession {
    oglContext_s context;
    Gerbera_s core;
    oglMatrixStack_s *stacks[GR_MATRIXMODE_MAX];
} DiffMatrixSession;

void diff_gl_matrix_destroy(DiffMatrixSession *session)
{
    int i;
    if (session == NULL) return;
    for (i = 0; i < GR_MATRIXMODE_MAX; ++i) free(session->stacks[i]);
    free(session);
}

DiffMatrixSession *diff_gl_matrix_create(unsigned int fill, const void *identity)
{
    DiffMatrixSession *session = (DiffMatrixSession *)calloc(1, sizeof(*session));
    int i;
    if (session == NULL) return NULL;
    session->context.gerbera = &session->core;
    oglSetDefaultGetterValues(&session->context.getValues);
    for (i = 0; i < GR_MATRIXMODE_MAX; ++i)
    {
        oglMatrixStack_s *stack = (oglMatrixStack_s *)malloc(DIFF_STACK_BYTES);
        if (stack == NULL) { diff_gl_matrix_destroy(session); return NULL; }
        memset(stack, (unsigned char)fill, DIFF_STACK_BYTES);
        stack->index = 0;
        stack->depth = DIFF_MATRIX_DEPTH;
        stack->dirty = 0;
        memcpy(stack->elements, identity, sizeof(_IMatrix4x4));
        session->stacks[i] = session->context.matrixStack[i] = stack;
    }
    return session;
}

uint32_t diff_gl_matrix_step(DiffMatrixSession *session, int operation, const uint32_t input[16])
{
    eglState_s state;
    void *previous = egliGetGlobal();
    float f[16];
    uint32_t result = 0;
    memset(&state, 0, sizeof(state));
    memcpy(f, input, sizeof(f));
    state.currentESContext = &session->context;
    egliSetGlobal(&state);
    switch (operation)
    {
    case DIFF_MATRIX_MODE: glMatrixMode(input[0]); break;
    case DIFF_ACTIVE_TEXTURE: glActiveTexture(input[0]); break;
    case DIFF_PALETTE_MATRIX: glCurrentPaletteMatrixOES(input[0]); break;
    case DIFF_LOAD_PALETTE: glLoadPaletteFromModelViewMatrixOES(); break;
    case DIFF_MATRIX_PUSH: glPushMatrix(); break;
    case DIFF_MATRIX_POP: glPopMatrix(); break;
    case DIFF_LOAD_IDENTITY: glLoadIdentity(); break;
    case DIFF_LOAD_FLOAT: glLoadMatrixf(f); break;
    case DIFF_LOAD_FIXED: glLoadMatrixx((const int *)input); break;
    case DIFF_MULT_FLOAT: glMultMatrixf(f); break;
    case DIFF_MULT_FIXED: glMultMatrixx((const int *)input); break;
    case DIFF_ROTATE_FLOAT: glRotatef(f[0], f[1], f[2], f[3]); break;
    case DIFF_ROTATE_FIXED: glRotatex((int)input[0], (int)input[1], (int)input[2], (int)input[3]); break;
    case DIFF_TRANSLATE_FLOAT: glTranslatef(f[0], f[1], f[2]); break;
    case DIFF_TRANSLATE_FIXED: glTranslatex((int)input[0], (int)input[1], (int)input[2]); break;
    case DIFF_SCALE_FLOAT: glScalef(f[0], f[1], f[2]); break;
    case DIFF_SCALE_FIXED: glScalex((int)input[0], (int)input[1], (int)input[2]); break;
    case DIFF_FRUSTUM_FLOAT: glFrustumf(f[0], f[1], f[2], f[3], f[4], f[5]); break;
    case DIFF_FRUSTUM_FIXED: glFrustumx((int)input[0], (int)input[1], (int)input[2],
                       (int)input[3], (int)input[4], (int)input[5]); break;
    case DIFF_ORTHO_FLOAT: glOrthof(f[0], f[1], f[2], f[3], f[4], f[5]); break;
    case DIFF_ORTHO_FIXED: glOrthox((int)input[0], (int)input[1], (int)input[2],
                     (int)input[3], (int)input[4], (int)input[5]); break;
    case DIFF_GET_ERROR: result = glGetError(); break;
    }
    egliSetGlobal(previous);
    return result;
}

void diff_gl_matrix_snapshot(DiffMatrixSession *session, unsigned char *output)
{
    int i;
    oglContext_s *context = &session->context;
    uint32_t pointers_changed = (context->gerbera != &session->core);
    typedef char context_control_layout[(offsetof(oglContext_s, drawSurfaceHeight) + sizeof(int) -
                                         offsetof(oglContext_s, displayOpen) == 20) ? 1 : -1];
    typedef char context_tail_layout[(offsetof(oglContext_s, rendererStr) + 48 -
                                      offsetof(oglContext_s, activeTextures) == 944) ? 1 : -1];
    (void)sizeof(context_control_layout);
    (void)sizeof(context_tail_layout);
    for (i = 0; i < GR_MATRIXMODE_MAX; ++i)
        pointers_changed |= (uint32_t)(context->matrixStack[i] != session->stacks[i]) << (i + 1);
    pointers_changed |= (uint32_t)(context->shareListPrev || context->shareListNext ||
        context->shareMemPrev || context->shareMemNext || context->textureManager ||
        context->bufObjManager || context->allocator.data ||
        context->workMemory || context->sharedWorkMem) << 14;
    memcpy(output, &pointers_changed, 4);
    memcpy(output + 4, &context->error, 4);
    memcpy(output + 8, &context->allocator.size, 8);
    memcpy(output + 16, &context->displayOpen, 20);
    memcpy(output + 36, &context->activeTextures, 944);
    for (i = 0; i < GR_MATRIXMODE_MAX; ++i)
        memcpy(output + DIFF_MATRIX_CONTEXT_BYTES + DIFF_STACK_BYTES * i,
               session->stacks[i], DIFF_STACK_BYTES);
}

/* Public fixed-function state operations share the persistent matrix fixture.
 * Core output uses the original ARM field offsets, excluding pointers. */
void diff_gl_state_snapshot(DiffMatrixSession *session, unsigned char output[0x3000])
{
    Gerbera_s *s = &session->core;
    int i;
    memset(output, 0, 0x3000);
#define BYTE(offset, member) output[offset] = (unsigned char)s->member
#define FIELD(offset, member) memcpy(output + (offset), &s->member, sizeof(s->member))
    BYTE(3, cullNegative); BYTE(4, frontFace); BYTE(5, cullMask);
    BYTE(6, fogMode); BYTE(24, lightingSideMask);
    memcpy(output + 8, s->normalsToEyePalDirty, 9); BYTE(20, normalsToEyeDirty);
    BYTE(25, lightActiveDirtyMask); BYTE(26, lightActiveMask);
    BYTE(27, lightTypeDirtyMask); BYTE(28, lightColorDirtyMask);
    BYTE(30, rejectAllStateDirty); BYTE(31, pixelPipelineDirty);
    BYTE(50, ambientLightSumDirty); BYTE(52, baseColorDirty);
    memcpy(output + 53, s->capabilities, sizeof(s->capabilities));
    FIELD(532, fogStart); FIELD(536, fogEnd); FIELD(540, fogDensity);
    FIELD(544, fogOORange); FIELD(548, fogColor);
    output[594] = (unsigned char)s->fogAtZero;
    output[595] = (unsigned char)(s->fogAtZero >> 8);
    FIELD(5520, material); FIELD(5576, ambientColor);
    for (i = 0; i < GR_VERTEXARRAY_MAX; ++i)
    {
        _grVertexArray *a = &s->vertexArray[i];
        uint32_t data = (uint32_t)(uintptr_t)a->data, pointer = (uint32_t)(uintptr_t)a->origPointer;
        unsigned char *d = output + 308 + 20 * i;
        memcpy(d, &a->stride, 4); d[4] = a->size; d[5] = a->type;
        memcpy(d + 8, &data, 4); memcpy(d + 12, &a->binding, 4);
        memcpy(d + 16, &pointer, 4);
    }
    for (i = 0; i < GR_MAX_TEXTURE_UNITS; ++i)
    {
        _grTextureUnitState *t = &s->textureUnit[2 * i];
        memcpy(output + 468 + 32 * i + 4, &t->envColor, 24);
        memcpy(output + 468 + 32 * i + 28, &t->texCoordReplace, 4);
    }
    for (i = 0; i < GR_MAX_LIGHTS; ++i)
    {
        _grLight *l = &s->lights[i];
        unsigned char *d = output + 5712 + 208 * i;
        memcpy(d, l->positionXYZ, 16);
        memcpy(d + 16, &l->spotCutoff, 4); memcpy(d + 20, &l->lightMask, 4);
        memcpy(d + 24, &l->orgPosition, 20); memcpy(d + 44, &l->orgSpotDirection, 16);
        memcpy(d + 60, l->spotDirectionNegated, 12); memcpy(d + 72, l->colors, 36);
        memcpy(d + 108, &l->exponent, 4); memcpy(d + 112, &l->attenuation, 16);
        memcpy(d + 128, &l->cosSpotCutoff, 4);
        memcpy(d + 168, l->halfway, 12); memcpy(d + 180, l->nVP, 12);
        memcpy(d + 192, &l->positional, 4);
        memcpy(d + 196, &l->xAtten, 4); memcpy(d + 200, &l->ndotL, 4);
        memcpy(d + 204, &l->specDot, 4);
    }
#undef BYTE
#undef FIELD
}

uint32_t diff_gl_state_step(DiffMatrixSession *session, int operation,
                           const uint32_t input[8], unsigned char *output)
{
    eglState_s state;
    void *previous = egliGetGlobal();
    const int *x = (const int *)(input + 2);
    float f[6];
    void *p = input[7] ? NULL : output;
    uint32_t result = 0;
    memcpy(f, input + 2, sizeof(f));
    memset(&state, 0, sizeof(state));
    state.currentESContext = &session->context;
    egliSetGlobal(&state);
    switch (operation)
    {
    case 0: glFogf(input[0], f[0]); break;
    case 1: glFogfv(input[0], f); break;
    case 2: glFogx(input[0], x[0]); break;
    case 3: glFogxv(input[0], x); break;
    case 4: glMaterialf(input[0], input[1], f[0]); break;
    case 5: glMaterialfv(input[0], input[1], f); break;
    case 6: glMaterialx(input[0], input[1], x[0]); break;
    case 7: glMaterialxv(input[0], input[1], x); break;
    case 8: glLightf(input[0], input[1], f[0]); break;
    case 9: glLightfv(input[0], input[1], f); break;
    case 10: glLightx(input[0], input[1], x[0]); break;
    case 11: glLightxv(input[0], input[1], x); break;
    case 12: glLightModelf(input[0], f[0]); break;
    case 13: glLightModelfv(input[0], f); break;
    case 14: glLightModelx(input[0], x[0]); break;
    case 15: glLightModelxv(input[0], x); break;
    case 16: glTexEnvf(input[0], input[1], f[0]); break;
    case 17: glTexEnvfv(input[0], input[1], input[7] ? NULL : f); break;
    case 18: glTexEnvx(input[0], input[1], x[0]); break;
    case 19: glTexEnvxv(input[0], input[1], input[7] ? NULL : x); break;
    case 20: glTexEnvi(input[0], input[1], x[0]); break;
    case 21: glTexEnviv(input[0], input[1], input[7] ? NULL : x); break;
    case 22: glGetLightfv(input[0], input[1], p); break;
    case 23: glGetLightxv(input[0], input[1], p); break;
    case 24: glGetMaterialfv(input[0], input[1], p); break;
    case 25: glGetMaterialxv(input[0], input[1], p); break;
    case 26: glGetTexEnvfv(input[0], input[1], p); break;
    case 27: glGetTexEnviv(input[0], input[1], p); break;
    case 28: glGetTexEnvxv(input[0], input[1], p); break;
    case 29: glGetFloatv(input[0], p); break;
    case 30: glGetFixedv(input[0], p); break;
    case 31: glGetIntegerv(input[0], p); break;
    case 32: glGetBooleanv(input[0], p); break;
    case 33: glActiveTexture(input[0]); break;
    case 34: result = glGetError(); break;
    case 35: glClientActiveTexture(input[0]); break;
    case 36: glEnableClientState(input[0]); break;
    case 37: glDisableClientState(input[0]); break;
    case 38: glVertexPointer((int)input[0], input[1], (int)input[2], (void *)(uintptr_t)input[3]); break;
    case 39: glColorPointer((int)input[0], input[1], (int)input[2], (void *)(uintptr_t)input[3]); break;
    case 40: glNormalPointer(input[0], (int)input[1], (void *)(uintptr_t)input[2]); break;
    case 41: glTexCoordPointer((int)input[0], input[1], (int)input[2], (void *)(uintptr_t)input[3]); break;
    case 42: glWeightPointerOES((int)input[0], input[1], (int)input[2], (void *)(uintptr_t)input[3]); break;
    case 43: glMatrixIndexPointerOES((int)input[0], input[1], (int)input[2], (void *)(uintptr_t)input[3]); break;
    case 44: glPointSizePointerOES(input[0], (int)input[1], (void *)(uintptr_t)input[2]); break;
    case 45:
    {
        uint32_t bits;
        void *pointer;
        memcpy(&bits, output, 4);
        pointer = (void *)(uintptr_t)bits;
        glGetPointerv(input[0], input[7] ? NULL : &pointer);
        bits = (uint32_t)(uintptr_t)pointer;
        memcpy(output, &bits, 4);
        break;
    }
    case 46: glEnable(input[0]); break;
    case 47: glDisable(input[0]); break;
    case 48: result = glIsEnabled(input[0]); break;
    }
    egliSetGlobal(previous);
    return result;
}
