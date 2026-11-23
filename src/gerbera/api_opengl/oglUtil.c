/* oglUtil.c - OpenGL ES enum mapping, input conversion, and state helpers
 * (port of oglUtil.inl; started with oglBinarySearch in session 5a). */
#include "gerbera/api_opengl/oglContext.h"

int oglBinarySearch(int key, int length, const unsigned short *keys)
{
    int l = 0;
    int r = length - 1;
    int i;

    for (i = 1; i < length; ++i)
    {
        OGL_ASSERT_AT("oglUtil.inl", 454,
                      "keys[i-1] < keys[i]",
                      keys[i - 1] < keys[i]);
    }
    do
    {
        int x = (r + l) >> 1;
        if (key >= (int)keys[x])
            l = x + 1;
        else
            r = x - 1;
        if (key == (int)keys[x])
            return x;
    }
    while (l <= r);
    return -1;
}
