/* test_core.c - checks for the gerbera core foundational layer (grDefs.h). */
#include "gerbera/grDefs.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, failures = 0;

#define CHECK(cond)                          \
    do                                       \
    {                                        \
        ++checks;                            \
        if (!(cond))                         \
        {                                    \
            ++failures;                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                    \
    } while (0)

static void test_descriptors(void)
{
    _grSurfaceDescriptor a, b;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    grClearDescriptor(&a);
    CHECK(a.rgbaOffsets[0] == 0 && a.rgbaSizes[0] == 0);
    CHECK(a.colorMask == 0 && a.colorBytes == 0 && a.depthBytes == 0);
    CHECK(a.stencilBytes == 0 && a.pixelFormat == 0);
    grSetDescriptorSizes(&a, 5, 6, 5, 8);
    CHECK(grGetDescriptorSize(&a, 0) == 5);
    CHECK(grGetDescriptorSize(&a, 1) == 6);
    CHECK(grGetDescriptorSize(&a, 3) == 8);
    grSetDescriptorOffsets(&a, 0, 5, 11, 16);
    CHECK(grGetDescriptorOffset(&a, 0) == 0);
    CHECK(grGetDescriptorOffset(&a, 2) == 11);
    grClearDescriptor(&b);
    CHECK(grDescriptorsEqual(&a, &b) == 0);
    grSetDescriptorSizes(&b, 5, 6, 5, 8);
    grSetDescriptorOffsets(&b, 0, 5, 11, 16);
    CHECK(grDescriptorsEqual(&a, &b) == 1);
    b.colorMask = 0xF0;
    CHECK(grDescriptorsEqual(&a, &b) == 0);
    b.colorMask = 0;
    CHECK(grDescriptorsEqual(&a, &b) == 1);
    b.pixelFormat = 3;
    CHECK(grDescriptorsEqual(&a, &b) == 0);
}

static void test_rectangles(void)
{
    _grRect r;
    grSetRectangle(&r, 0, 100, 0, 200);
    CHECK(grIsRectangleEmpty(&r) == 0);
    grSetRectangle(&r, 10, 10, 0, 200);
    CHECK(grIsRectangleEmpty(&r) == 1);
    grSetRectangle(&r, 0, 100, 10, 10);
    CHECK(grIsRectangleEmpty(&r) == 1);
    CHECK(grIsAligned((void *)0x1000, 4) == 1);
    CHECK(grIsAligned((void *)0x1002, 4) == 0);
    CHECK(grIsAligned((void *)0x1003, 4) == 0);
    CHECK(grIsAligned((void *)0x1004, 4) == 1);
}

static void test_formats(void)
{
    /* data type size table verbatim */
    CHECK(grGetDataTypeSize(0) == 1);
    CHECK(grGetDataTypeSize(1) == 1);
    CHECK(grGetDataTypeSize(2) == 2);
    CHECK(grGetDataTypeSize(3) == 2);
    CHECK(grGetDataTypeSize(4) == 4);
    CHECK(grGetDataTypeSize(5) == 4);
    CHECK(grGetDataTypeSize(6) == 4);
    CHECK(grGetDataTypeSize(7) == 4);
    CHECK(grGetDataTypeSize(8) == 1);
    CHECK(grGetDataTypeSize(9) == 0);
    /* image format component counts: {1,3,4,1,2,3,4} */
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_ALPHA) == 1);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_RGB) == 3);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_RGBA) == 4);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_LUMINANCE) == 1);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_LUMINANCE_ALPHA) == 2);
    CHECK(grGetImageFormatNumComponents(GR_IMAGEFORMAT_BGR) == 3);
    CHECK(grGetImageFormatNumComponents(6) == 4);
}

static void test_stack(void)
{
    _grStackAllocator st;
    static char buf[256];
    char *p, *q;

    grStackInit(&st, buf, 256);
    CHECK(grIsStackValid(&st) == 1);
    CHECK(grStackGetFreeSpace(&st) == 256);
    p = (char *)grStackAllocate(&st, 17);
    CHECK(p == buf);
    CHECK(grIsAligned(p, 4) == 1);
    CHECK(grStackGetFreeSpace(&st) == 256 - 20);   /* 17 rounded up to 20 */
    q = (char *)grStackAllocate(&st, 40);
    CHECK(q == buf + 20);
    CHECK(grStackGetFreeSpace(&st) == 256 - 60);
    grStackFree(&st, q);
    CHECK(grStackGetFreeSpace(&st) == 256 - 20);
    grStackFree(&st, p);
    CHECK(grStackGetFreeSpace(&st) == 256);
    CHECK(grStackGetPtr(&st, 128) == buf + 128);
}

int main(void)
{
    test_descriptors();
    test_rectangles();
    test_formats();
    test_stack();
    printf("core tests: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
