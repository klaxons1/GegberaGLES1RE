/* oglError.c - OpenGL ES error recording (port of oglError.inl). */
#include "gerbera/api_opengl/oglContext.h"

void oglSetError(oglContext_s *context, OGL_ERROR error)
{
    OGL_ASSERT_AT("oglError.inl", 34,
                  "hgInRange32((int)error, OGL_ERROR_OK, OGL_ERROR_MAX-1)",
                  hgInRange32((int)error, OGL_ERROR_OK, OGL_ERROR_MAX - 1) != 0);
    if (context->error == OGL_ERROR_OK)
        context->error = error;
}
