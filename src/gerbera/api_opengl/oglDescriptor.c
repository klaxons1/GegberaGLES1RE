/* oglDescriptor.c - packed 16-bit GL pixel descriptor builder
 * (port of oglDescriptor.inl). */
#include "gerbera/api_opengl/oglContext.h"

void oglFillDescriptor(_grSurfaceDescriptor *desc, unsigned int format, unsigned int type)
{
    OGL_ASSERT_AT("oglDescriptor.inl", 39, "desc", desc != NULL);
    grClearDescriptor(desc);
    if (format == GL_RGB)
    {
        OGL_ASSERT_AT("oglDescriptor.inl", 44,
                      "type == GL_UNSIGNED_SHORT_5_6_5",
                      type == GL_UNSIGNED_SHORT_5_6_5);
        grSetDescriptorSizes(desc, 5, 6, 5, 0);
        grSetDescriptorOffsets(desc, 11, 5, 0, 0);
        desc->pixelFormat = 4;
        desc->colorMask   = 7;
    }
    else
    {
        OGL_ASSERT_AT("oglDescriptor.inl", 52,
                      "format == GL_RGBA",
                      format == GL_RGBA);
        if (type == GL_UNSIGNED_SHORT_4_4_4_4)
        {
            grSetDescriptorSizes(desc, 4, 4, 4, 4);
            grSetDescriptorOffsets(desc, 12, 8, 4, 0);
        }
        else
        {
            OGL_ASSERT_AT("oglDescriptor.inl", 60,
                          "type == GL_UNSIGNED_SHORT_5_5_5_1",
                          type == GL_UNSIGNED_SHORT_5_5_5_1);
            grSetDescriptorSizes(desc, 5, 5, 5, 1);
            grSetDescriptorOffsets(desc, 11, 6, 1, 0);
        }
        desc->colorMask   = 15;
        desc->pixelFormat = 5;
    }
    desc->colorBytes = 2;
}
