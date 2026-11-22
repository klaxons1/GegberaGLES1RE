/* grRGBA.c - port of the decompiled grRGBA.h inline functions. */
#include "gerbera/grRGBA.h"
#include "hg/hgInt32.h"

unsigned int grSetRGBA(int r, int g, int b, int a)
{
    GR_RGBA_CHECK(hgInRange32(r, 0, 255), "grRGBA.h", 32);
    GR_RGBA_CHECK(hgInRange32(g, 0, 255), "grRGBA.h", 33);
    GR_RGBA_CHECK(hgInRange32(b, 0, 255), "grRGBA.h", 34);
    GR_RGBA_CHECK(hgInRange32(a, 0, 255), "grRGBA.h", 35);
    return ((unsigned int)a << 24) | (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16);
}

int grUnpackRGBA(unsigned int color, int ndx)
{
    GR_RGBA_CHECK(hgInRange32(ndx, 0, GR_COLOR_MAX), "grRGBA.h", 41);
    return (int)(unsigned char)(color >> (8 * ndx));
}

unsigned int grPackRGBA(int component, int ndx)
{
    GR_RGBA_CHECK(hgInRange32(ndx, 0, GR_COLOR_MAX), "grRGBA.h", 47);
    GR_RGBA_CHECK(hgInRange32(component, 0, 255), "grRGBA.h", 48);
    return (unsigned int)component << (8 * ndx);
}
