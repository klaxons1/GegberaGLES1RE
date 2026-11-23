/* oglMatrixStack.c - OpenGL ES matrix stack helpers
 * (port of oglMatrixStack.inl). */
#include "gerbera/api_opengl/oglContext.h"

int oglIsMatrixStackValid(oglMatrixStack_s *stack)
{
    if (stack == NULL)
        return 0;
    if (stack->dirty < 2u)
        return hgInRange32(stack->index, 0, (int)stack->depth - 1) != 0;
    return 0;
}

oglMatrixStack_s *oglGetMatrixStack(oglContext_s *context, int matrixMode)
{
    int i;

    for (i = 0; i < GR_MATRIXMODE_MAX; ++i)
    {
        OGL_ASSERT_AT("oglMatrixStack.inl", 59,
                      "oglIsMatrixStackValid(context->matrixStack[i])",
                      oglIsMatrixStackValid(context->matrixStack[i]) != 0);
    }
    OGL_ASSERT_AT("oglMatrixStack.inl", 62,
                  "hgInRange32((int)matrixMode, 0, GR_MATRIXMODE_MAX-1)",
                  hgInRange32(matrixMode, 0, GR_MATRIXMODE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglMatrixStack.inl", 63,
                  "hgInRange32(context->activeTexture, 0, GR_MAX_TEXTURE_UNITS-1)",
                  hgInRange32(context->activeTexture, 0, GR_MAX_TEXTURE_UNITS - 1) != 0);
    return context->matrixStack[matrixMode];
}

oglMatrixStack_s *oglGetActiveMatrixStack(oglContext_s *context)
{
    return oglGetMatrixStack(context, (int)context->matrixMode);
}

_IMatrix4x4 *oglGetActiveMatrix(oglContext_s *context, int setDirty)
{
    oglMatrixStack_s *stack;

    stack = oglGetActiveMatrixStack(context);
    OGL_ASSERT_AT("oglMatrixStack.inl", 100,
                  "hgInRange32((int)context->matrixMode, 0, GR_MATRIXMODE_MAX-1)",
                  hgInRange32((int)context->matrixMode, 0, GR_MATRIXMODE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglMatrixStack.inl", 101,
                  "hgInRange32(context->activeTexture, 0, GR_MAX_TEXTURE_UNITS-1)",
                  hgInRange32(context->activeTexture, 0, GR_MAX_TEXTURE_UNITS - 1) != 0);
    OGL_ASSERT_AT("oglMatrixStack.inl", 102, "stack", stack != NULL);
    stack->dirty |= (unsigned char)setDirty;
    return &stack->elements[stack->index];
}

_IMatrix4x4 *oglGetMatrix(oglContext_s *context, int matrixMode)
{
    oglMatrixStack_s *stack = oglGetMatrixStack(context, matrixMode);
    return &stack->elements[stack->index];
}

void oglMatrixToOutput(void *output, _IMatrix4x4 *matrix, OGL_OUTPUTTYPE dataType)
{
    float fArr[16];

    IMatrix4x4_getfv((int *)fArr, matrix);
    oglFloatToOutput(output, fArr, 16, dataType);
}

void oglMatrixFixedToFloat(float *dst, const int *src)
{
    int i;

    for (i = 0; i < 16; ++i)
    {
        int s = IScalar_fixedToScalar(src[i], 16);
        dst[i] = IScalar_stof(s);
    }
}
