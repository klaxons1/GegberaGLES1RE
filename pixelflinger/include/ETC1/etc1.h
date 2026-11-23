#ifndef PIXELFLINGER_SHIM_ETC1_H
#define PIXELFLINGER_SHIM_ETC1_H

#include <stdint.h>

typedef unsigned char etc1_byte;
typedef int           etc1_bool;
typedef uint32_t      etc1_uint32;

static inline etc1_uint32 etc1_get_encoded_data_size(etc1_uint32 width, etc1_uint32 height)
{
    return (((width + 3u) & ~3u) * ((height + 3u) & ~3u)) >> 1;
}

static inline etc1_byte etc1_clamp255(int x)
{
    return (x < 0) ? 0u : ((x > 255) ? 255u : (etc1_byte)x);
}

static inline void etc1_decode_subblock(etc1_byte *pOut, int r, int g, int b,
                                        const short *table, uint32_t low,
                                        int second, int flipped,
                                        etc1_uint32 width, etc1_uint32 height,
                                        etc1_uint32 bx, etc1_uint32 by,
                                        etc1_uint32 pixelSize, etc1_uint32 stride)
{
    int x0 = flipped ? 0 : (second ? 2 : 0);
    int x1 = flipped ? 4 : (second ? 4 : 2);
    int y0 = flipped ? (second ? 2 : 0) : 0;
    int y1 = flipped ? (second ? 4 : 2) : 4;

    for (int y = y0; y < y1; ++y) {
        etc1_uint32 py = by + (etc1_uint32)y;
        if (py >= height)
            continue;
        for (int x = x0; x < x1; ++x) {
            etc1_uint32 px = bx + (etc1_uint32)x;
            if (px >= width)
                continue;
            int bitIdx = x * 4 + y;
            int lsb = (int)((low >> bitIdx) & 1u);
            int msb = (int)((low >> (bitIdx + 16)) & 1u);
            int delta = table[lsb];
            if (msb)
                delta = -delta;
            etc1_byte *dst = pOut + py * stride + px * pixelSize;
            dst[0] = etc1_clamp255(r + delta);
            dst[1] = etc1_clamp255(g + delta);
            dst[2] = etc1_clamp255(b + delta);
            if (pixelSize == 4u)
                dst[3] = 255u;
        }
    }
}

static inline int etc1_decode_image(const etc1_byte *pIn, etc1_byte *pOut,
                                    etc1_uint32 width, etc1_uint32 height,
                                    etc1_uint32 pixelSize, etc1_uint32 stride)
{
    static const short kModifierTable[8][2] = {
        {  2,   8 }, {  5,  17 }, {  9,  29 }, { 13,  42 },
        { 18,  60 }, { 24,  80 }, { 33, 106 }, { 47, 183 }
    };

    if ((pixelSize != 3u && pixelSize != 4u) || !pIn || !pOut)
        return -1;

    for (etc1_uint32 by = 0; by < height; by += 4u) {
        for (etc1_uint32 bx = 0; bx < width; bx += 4u) {
            uint32_t high = ((uint32_t)pIn[0] << 24) | ((uint32_t)pIn[1] << 16) |
                            ((uint32_t)pIn[2] << 8)  | (uint32_t)pIn[3];
            uint32_t low  = ((uint32_t)pIn[4] << 24) | ((uint32_t)pIn[5] << 16) |
                            ((uint32_t)pIn[6] << 8)  | (uint32_t)pIn[7];
            pIn += 8;

            int r1, r2, g1, g2, b1, b2;
            if (high & 2u) {
                int rBase = (int)((high >> 27) & 0x1Fu);
                int gBase = (int)((high >> 19) & 0x1Fu);
                int bBase = (int)((high >> 11) & 0x1Fu);
                int dr = ((int)((high >> 24) & 7u) << 29) >> 29;
                int dg = ((int)((high >> 16) & 7u) << 29) >> 29;
                int db = ((int)((high >>  8) & 7u) << 29) >> 29;
                int r2c = rBase + dr, g2c = gBase + dg, b2c = bBase + db;
                r1 = (rBase << 3) | (rBase >> 2);
                g1 = (gBase << 3) | (gBase >> 2);
                b1 = (bBase << 3) | (bBase >> 2);
                r2 = (r2c << 3) | ((r2c >> 2) & 7);
                g2 = (g2c << 3) | ((g2c >> 2) & 7);
                b2 = (b2c << 3) | ((b2c >> 2) & 7);
            } else {
                int r1n = (int)((high >> 28) & 0xFu), r2n = (int)((high >> 24) & 0xFu);
                int g1n = (int)((high >> 20) & 0xFu), g2n = (int)((high >> 16) & 0xFu);
                int b1n = (int)((high >> 12) & 0xFu), b2n = (int)((high >>  8) & 0xFu);
                r1 = (r1n << 4) | r1n; r2 = (r2n << 4) | r2n;
                g1 = (g1n << 4) | g1n; g2 = (g2n << 4) | g2n;
                b1 = (b1n << 4) | b1n; b2 = (b2n << 4) | b2n;
            }

            int t1 = (int)((high >> 5) & 7u);
            int t2 = (int)((high >> 2) & 7u);
            int flip = (int)(high & 1u);

            etc1_decode_subblock(pOut, r1, g1, b1, kModifierTable[t1], low, 0, flip,
                                 width, height, bx, by, pixelSize, stride);
            etc1_decode_subblock(pOut, r2, g2, b2, kModifierTable[t2], low, 1, flip,
                                 width, height, bx, by, pixelSize, stride);
        }
    }
    return 0;
}

#endif /* PIXELFLINGER_SHIM_ETC1_H */
