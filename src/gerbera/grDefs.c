/* grDefs.c - port of the inline functions of the original grDefs.h.
 *
 * The original header's functions were inlined (2-3 identical TU copies in
 * the binary); a single definition is ported here. Assert sites keep the
 * original grDefs.h file/line strings.
 */
#include "gerbera/grDefs.h"
#include "hg/hgInt32.h"

/* grGetDataTypeSize_dataTypeSizes - verbatim from docs/libGLES_CM.dll.c:5690 */
const char grGetDataTypeSize_dataTypeSizes[12] = { 1, 1, 2, 2, 4, 4, 4, 4, 1, 0, 0, 0 };

/* --- descriptor helpers -------------------------------------------------- */

void grClearDescriptor(_grSurfaceDescriptor *a)
{
    GR_DEFS_CHECK(a != NULL, "grDefs.h", 219);
    *(int *)a->rgbaOffsets = 0;
    *(int *)a->rgbaSizes = 0;
    a->colorMask = 0;
    a->colorBytes = 0;
    a->depthBytes = 0;
    a->stencilBytes = 0;
    *(int *)&a->pixelFormat = 0;
}

void grSetDescriptorSizes(_grSurfaceDescriptor *desc, int rSize, int gSize, int bSize, int aSize)
{
    GR_DEFS_CHECK(rSize >= 0 && rSize <= 15, "grDefs.h", 275);
    GR_DEFS_CHECK(gSize >= 0 && gSize <= 15, "grDefs.h", 276);
    GR_DEFS_CHECK(bSize >= 0 && bSize <= 15, "grDefs.h", 277);
    GR_DEFS_CHECK(aSize >= 0 && aSize <= 15, "grDefs.h", 278);
    desc->rgbaSizes[0] = (unsigned char)rSize;
    desc->rgbaSizes[1] = (unsigned char)gSize;
    desc->rgbaSizes[2] = (unsigned char)bSize;
    desc->rgbaSizes[3] = (unsigned char)aSize;
}

void grSetDescriptorOffsets(_grSurfaceDescriptor *desc, int rOffset, int gOffset, int bOffset, int aOffset)
{
    GR_DEFS_CHECK(rOffset >= 0 && rOffset <= 31, "grDefs.h", 287);
    GR_DEFS_CHECK(gOffset >= 0 && gOffset <= 31, "grDefs.h", 288);
    GR_DEFS_CHECK(bOffset >= 0 && bOffset <= 31, "grDefs.h", 289);
    GR_DEFS_CHECK(aOffset >= 0 && aOffset <= 31, "grDefs.h", 290);
    desc->rgbaOffsets[0] = (unsigned char)rOffset;
    desc->rgbaOffsets[1] = (unsigned char)gOffset;
    desc->rgbaOffsets[2] = (unsigned char)bOffset;
    desc->rgbaOffsets[3] = (unsigned char)aOffset;
}

int grDescriptorsEqual(_grSurfaceDescriptor *a, _grSurfaceDescriptor *b)
{
    GR_DEFS_CHECK(!a->padding[0] && !a->padding[1], "grDefs.h", 236);
    GR_DEFS_CHECK(!b->padding[0] && !b->padding[1], "grDefs.h", 237);
    /* whole-int compares like the decompile */
    if (*(int *)a->rgbaOffsets == *(int *)b->rgbaOffsets &&
        *(int *)a->rgbaSizes == *(int *)b->rgbaSizes &&
        *(int *)&a->colorMask == *(int *)&b->colorMask)
    {
        if (*(int *)&a->pixelFormat == *(int *)&b->pixelFormat)
            return 1;
    }
    return 0;
}

int grGetDescriptorSize(_grSurfaceDescriptor *desc, int channel)
{
    GR_DEFS_CHECK(channel >= 0 && channel <= 3, "grDefs.h", 255);
    return desc->rgbaSizes[channel];
}

int grGetDescriptorOffset(_grSurfaceDescriptor *desc, int channel)
{
    GR_DEFS_CHECK(channel >= 0 && channel <= 3, "grDefs.h", 269);
    return desc->rgbaOffsets[channel];
}

/* --- rectangles ----------------------------------------------------------- */

void grSetRectangle(_grRect *rect, int x0, int x1, int y0, int y1)
{
    GR_DEFS_CHECK(rect != NULL, "grDefs.h", 322);
    rect->x0 = x0;
    rect->x1 = x1;
    rect->y0 = y0;
    rect->y1 = y1;
}

int grIsRectangleEmpty(_grRect *rect)
{
    GR_DEFS_CHECK(rect != NULL, "grDefs.h", 306);
    if (rect->x1 != rect->x0)
        return rect->y1 == rect->y0;
    return 1;
}

/* --- data types / image formats ------------------------------------------- */

int grGetDataTypeSize(int dataType)
{
    GR_DEFS_CHECK((unsigned int)dataType < GR_DATATYPE_MAX, "grDefs.h", 359);
    return (unsigned char)grGetDataTypeSize_dataTypeSizes[dataType];
}

int grGetImageFormatNumComponents(int imageFormat)
{
    unsigned int result = (0x11A319u >> (3 * imageFormat)) & 7;

    GR_DEFS_CHECK(hgInRange32(imageFormat, 0, GR_IMAGEFORMAT_MAX - 1), "grDefs.h", 384);
    switch (imageFormat)
    {
    case GR_IMAGEFORMAT_ALPHA:
        GR_DEFS_CHECK(result == 1, "grDefs.h", 387);
        break;
    case GR_IMAGEFORMAT_RGB:
        GR_DEFS_CHECK(result == 3, "grDefs.h", 388);
        break;
    case GR_IMAGEFORMAT_RGBA:
        GR_DEFS_CHECK(result == 4, "grDefs.h", 389);
        break;
    case GR_IMAGEFORMAT_LUMINANCE:
        GR_DEFS_CHECK(result == 1, "grDefs.h", 390);
        break;
    case GR_IMAGEFORMAT_LUMINANCE_ALPHA:
        GR_DEFS_CHECK(result == 2, "grDefs.h", 391);
        break;
    case GR_IMAGEFORMAT_BGR:
        GR_DEFS_CHECK(result == 3, "grDefs.h", 392);
        break;
    default:
        GR_DEFS_CHECK(result == 4, "grDefs.h", 395);
        break;
    }
    return (int)result;
}

int grIsRectangleInRange(_grRect *rect, int mn, int mx)
{
    GR_DEFS_CHECK(rect != NULL, "grDefs.h", 341);
    if (hgInRange32(rect->x0, mn, mx) == 0)
        return 0;
    if (hgInRange32(rect->x1, mn, mx) == 0)
        return 0;
    if (hgInRange32(rect->y0, mn, mx) != 0)
        return hgInRange32(rect->y1, mn, mx) != 0;
    return 0;
}

/* --- stack allocator ------------------------------------------------------- */

int grIsStackValid(_grStackAllocator *allocator)
{
    int ok = 1;
    if (!(allocator != NULL && allocator->data != NULL) && grAssert("allocator && allocator->data", "grDefs.h", 443) != 0)
        ok = 0;
    GR_DEFS_CHECK(ok, "grDefs.h", 443);
    GR_DEFS_CHECK(allocator->size > 0, "grDefs.h", 444);
    if (!(allocator->allocated >= 0 && allocator->allocated <= allocator->size) &&
        grAssert("allocator->allocated>=0 && allocator->allocated<=allocator->size", "grDefs.h", 445) != 0)
        ok = 0;
    GR_DEFS_CHECK(ok, "grDefs.h", 445);
    return 1;
}

int grStackGetFreeSpace(_grStackAllocator *allocator)
{
    GR_DEFS_CHECK(grIsStackValid(allocator) != 0, "grDefs.h", 455);
    GR_DEFS_CHECK(allocator != NULL, "grDefs.h", 456);
    return allocator->size - allocator->allocated;
}

void grStackInit(_grStackAllocator *allocator, void *data, int size)
{
    allocator->data = data;
    allocator->size = size;
    allocator->allocated = 0;
    GR_DEFS_CHECK(grIsStackValid(allocator) != 0, "grDefs.h", 466);
}

void *grStackGetPtr(_grStackAllocator *allocator, int offset)
{
    GR_DEFS_CHECK(allocator != NULL, "grDefs.h", 471);
    GR_DEFS_CHECK(grIsStackValid(allocator) != 0, "grDefs.h", 472);
    return (char *)allocator->data + offset;
}

void *grStackAllocate(_grStackAllocator *allocator, int size)
{
    void *ptr;
    unsigned int sizea;

    GR_DEFS_CHECK(allocator != NULL, "grDefs.h", 478);
    GR_DEFS_CHECK(grIsStackValid(allocator) != 0, "grDefs.h", 479);
    GR_DEFS_CHECK(grStackGetFreeSpace(allocator) >= size, "grDefs.h", 480);
    ptr = grStackGetPtr(allocator, allocator->allocated);
    GR_DEFS_CHECK(grIsAligned(ptr, 4) != 0, "grDefs.h", 483);
    sizea = (unsigned int)(size + 3) & 0xFFFFFFFCu;
    GR_DEFS_CHECK(allocator->size >= (int)(sizea + allocator->allocated), "grDefs.h", 486);
    allocator->allocated += (int)sizea;
    return ptr;
}

void grStackFree(_grStackAllocator *allocator, void *ptr)
{
    int ok = 1;
    if (!(allocator != NULL && ptr != NULL) && grAssert("allocator && ptr", "grDefs.h", 495) != 0)
        ok = 0;
    GR_DEFS_CHECK(ok, "grDefs.h", 495);
    GR_DEFS_CHECK(grIsStackValid(allocator) != 0, "grDefs.h", 496);
    if (!(ptr >= grStackGetPtr(allocator, 0) && ptr <= grStackGetPtr(allocator, allocator->size)) &&
        grAssert("ptr >= grStackGetPtr(allocator, 0) && ptr <= grStackGetPtr(allocator, allocator->size)",
                 "grDefs.h", 498) != 0)
        ok = 0;
    GR_DEFS_CHECK(ok, "grDefs.h", 498);
    GR_DEFS_CHECK(ptr < grStackGetPtr(allocator, allocator->allocated), "grDefs.h", 500);
    allocator->allocated = (int)((char *)ptr - (char *)grStackGetPtr(allocator, 0));
    GR_DEFS_CHECK(grIsStackValid(allocator) != 0, "grDefs.h", 503);
}
