/* grRasterizer.c - per-triangle gradient setup (port of the decompiled
 * grRasterizer.inl gradient block).
 *
 * grSetupTriangleGradients converts the three vertex records of a queued
 * triangle (render stack vertex buffer, 128-byte records) into per-pixel
 * step rates in the pixel-pipe record: reference[] holds the attribute value
 * at screen origin, xAdder[]/yAdder[] the per-pixel steps, both indexed by
 * GR_GRADIENT_*. Depth and fog go through grSetupGradient (which also
 * rebuilds the reference from the v0 screen offset), texcoord gradients go
 * through grComputeDeltas + grSetupTextureUnit (texture unit session), and
 * colors use grSetupGradient or the flat-shade direct writes depending on
 * GR_CAPABILITY_SMOOTH_SHADING.
 *
 * Assert sites keep the original file/line strings.
 */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "imath/imApi.h"
#include "hg/hgInt32.h"
#include <string.h>

int grSubpixelTrunc(int i);   /* grClipper.c (grUtil.h; DLL 004214A0) */

#define GR_PIXEL_PIPE(state) ((_grPixelPipe *)(state)->pixelPipeline)
#define GR_VERTEX_BUFFER(state) \
    ((_grVertex *)((unsigned char *)(state)->renderStack + 1332))

/* --- grSetupGradient (grRasterizer.inl 500; DLL 25988-26026) ---------------- */

void grSetupGradient(_grGradientInfo *info, int gradient, int g01, int g02,
                     int reference)
{
    int xAdd32 = 0;
    int yAdd32 = 0;

    if (hgInRange32(info->shift, 0, 31) == 0)
        imAssert("hgInRange32(info->shift, 0, 31)", "grRasterizer.inl", 500);
    if (info == NULL)
        imAssert("info", "grRasterizer.inl", 501);
    if (g01 != 0 || g02 != 0)   /* __PAIR64__(g02, g01) != 0 */
    {
        HGint64 xAdd64 = hgMul64(info->v01syPerArea, g02);
        HGint64 xAdd64a;
        HGint64 xAdd64b;
        HGint64 yAdd64 = hgMul64(info->v02sxPerArea, g01);
        HGint64 yAdd64a;
        HGint64 yAdd64b;

        xAdd64a = hgAsr64(hgMadd64(xAdd64, info->v02syPerArea, -g01),
                          info->shift + 14);
        xAdd32 = hgGet64l(xAdd64a);
        xAdd64b = hgMul64x32(xAdd64a, info->v0sx);
        yAdd64a = hgAsr64(hgMadd64(yAdd64, info->v01sxPerArea, -g02),
                          info->shift + 14);
        yAdd32 = hgGet64l(yAdd64a);
        yAdd64b = hgMul64x32(yAdd64a, info->v0sy);
        reference -= hgLsl64h_0_31(hgAdd64(xAdd64b, yAdd64b), 20);
    }
    info->pPP->reference[gradient] = reference;
    info->pPP->xAdder[gradient] = xAdd32;
    info->pPP->yAdder[gradient] = yAdd32;
}

/* --- grComputeDeltas (grRasterizer.inl 550; DLL 26029-26061) -----------------
 * Same plane-gradient solve as grSetupGradient but keeps full precision in
 * the adders (the mip-level estimate in grSetupTextureUnit consumes them
 * before they are re-shifted per wrap mode). The >= 32 / < 32 split computes
 * (syDelta >> (shift+14)) from the high word with a compensating left shift. */

void grComputeDeltas(_grGradientInfo *info, int gradient, int g01, int g02)
{
    int xAdder = 0;
    int yAdder = 0;

    if (hgInRange32(info->shift, 0, 31) == 0)
        imAssert("hgInRange32(info->shift, 0, 31)", "grRasterizer.inl", 550);
    if (info == NULL)
        imAssert("info", "grRasterizer.inl", 551);
    if (g01 != 0 || g02 != 0)
    {
        HGint64 syDelta = hgMul64(info->v01syPerArea, g02);
        HGint64 sxDelta;

        syDelta = hgMadd64(syDelta, info->v02syPerArea, -g01);
        sxDelta = hgMul64(info->v02sxPerArea, g01);
        sxDelta = hgMadd64(sxDelta, info->v01sxPerArea, -g02);
        if (info->shift + 14 >= 32)
        {
            int sh = info->shift - 18;
            xAdder = hgGet64h(syDelta) >> sh;
            yAdder = hgGet64h(sxDelta) >> sh;
        }
        else
        {
            int sh = 32 - (info->shift + 14);
            xAdder = hgLsl64h_0_31(syDelta, sh);
            yAdder = hgLsl64h_0_31(sxDelta, sh);
        }
    }
    info->pPP->xAdder[gradient] = xAdder;
    info->pPP->yAdder[gradient] = yAdder;
}

/* --- grSetupTriangleGradients (grRasterizer.inl 603; DLL 26064-26248) --------
 *
 * gradientMask bits (state->activeGradientMask convention): 0x1 depth,
 * 0x2 fog, 0x3C RGBA (4<<comp), 0xC0 tex0 s/t, 0x300 tex1 s/t, 0x20 alpha
 * (inside the 0x3C walk). The texcoord walk runs texIndex 1..0 and, per
 * unit, coord 1..0 (T then S) - gradient index = coord + 2*texIndex + 6.
 */

void grSetupTriangleGradients(Gerbera_s *state, _grClipTriangle *tri,
                              int flatShadeIndex, unsigned int gradientMask)
{
    int ok = 1;

    if ((state == NULL || tri == NULL) && grAssert("state && tri", "grRasterizer.inl", 603) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    /* compiled bound is the literal 67 (= 64 + 4 - 1 in the rasterizer TU's
     * header scope; the clipper TU pins the rasterizer cache at 28 records
     * with its temp base at 92 - the original headers disagree on the macro,
     * the value wins for behavior, the string stays verbatim) */
    if (hgInRange32(flatShadeIndex, 0, 67) == 0 &&
        grAssert("hgInRange32(flatShadeIndex, 0, GR_VERTEX_CACHE_SIZE + GR_NUM_RASTERIZER_CACHE_VERTICES - 1)",
                 "grRasterizer.inl", 606) != 0)
        grDie(0);
    if (gradientMask != 0)
    {
        _grPixelPipe *pixelPipe = GR_PIXEL_PIPE(state);
        _grVertex *vertexBuffer = GR_VERTEX_BUFFER(state);
        _grVertex *v0 = vertexBuffer + grUnpackIndex(tri->indices, 0);
        _grVertex *v1 = vertexBuffer + grUnpackIndex(tri->indices, 1);
        _grVertex *v2 = vertexBuffer + grUnpackIndex(tri->indices, 2);
        HGint64 reciprocal;
        int recipShift;
        int signedRecip;
        int v0sx;
        int v0sy;
        _grGradientInfo info;
        int texIndex;
        int coord;

        ok = 1;
        if (tri->area == 0 && grAssert("tri->area != 0", "grRasterizer.inl", 628) != 0)
            ok = 0;
        if (ok == 0)
            grDie(0);

        /* imRcpu(abs(area)): low word = normalization shift, high word =
         * reciprocal mantissa (0.32) */
        reciprocal = imRcpu((unsigned int)hgAbs32(tri->area));
        recipShift = hgGet64l(reciprocal);
        signedRecip = hgCopySign32(hgGet64h(reciprocal), -tri->area);
        info.pPP = pixelPipe;
        info.shift = 31 - recipShift;
        if (hgInRange32(recipShift, 0, 31) == 0 &&
            grAssert("hgInRange32(reciprocal.shift, 0, 31)",
                     "grRasterizer.inl", 641) != 0)
            grDie(0);

        /* edge derivatives: 16 * screen delta * signed reciprocal (high
         * half); v0 screen coords keep the raw record values for the deltas
         * and the -4096-centered ones for the reference rebuild */
        info.v01sxPerArea = hgMul64h(16 * (v1->screenPosition[0] - v0->screenPosition[0]), signedRecip);
        info.v01syPerArea = hgMul64h(16 * (v1->screenPosition[1] - v0->screenPosition[1]), signedRecip);
        info.v02sxPerArea = hgMul64h(16 * (v2->screenPosition[0] - v0->screenPosition[0]), signedRecip);
        info.v02syPerArea = hgMul64h(16 * (v2->screenPosition[1] - v0->screenPosition[1]), signedRecip);
        v0sx = v0->screenPosition[0] - 4096;
        v0sy = v0->screenPosition[1] - 4096;
        info.v0sx = v0sx;
        info.v0sy = v0sy;

        /* texcoord gradients (texIndex 1..0) */
        for (texIndex = GR_GRADIENT_FOG; texIndex >= GR_GRADIENT_DEPTH; --texIndex)
        {
            if (((192 << (2 * texIndex)) & gradientMask) != 0)
            {
                int baseValue[2];   /* shifted v0 texcoord per coord */
                int preShift[2];    /* clamped normalization shift - 1 */
                int *tc0 = &v0->texCoord[texIndex].x;
                int *tc1 = &v1->texCoord[texIndex].x;
                int *tc2 = &v2->texCoord[texIndex].x;
                int pairs[2][2];

                for (coord = GR_GRADIENT_FOG; coord >= GR_GRADIENT_DEPTH; --coord)
                {
                    int d0 = tc0[coord];
                    int d1 = tc1[coord];
                    int d2 = tc2[coord];
                    int shift;

                    shift = hgClz32(hgAbs32(d2) | (hgAbs32(d0) | hgAbs32(d1))) - 1;
                    {
                        int clamped = hgClamp32_0_x(shift, 11);
                        d0 <<= clamped;
                        d1 <<= clamped;
                        d2 <<= clamped;
                        d0 >>= 1;
                        d1 >>= 1;
                        d2 >>= 1;
                        preShift[coord] = clamped - 1;
                    }
                    grComputeDeltas(&info, coord + 2 * texIndex + 6,
                                    d1 - d0, d2 - d0);
                    baseValue[coord] = d0;
                }
                pairs[0][0] = baseValue[0];
                pairs[0][1] = preShift[0];
                pairs[1][0] = baseValue[1];
                pairs[1][1] = preShift[1];
                grSetupTextureUnit(state, texIndex, pairs, v0sx, v0sy);
            }
        }

        /* depth gradient (+ depth offset when the polygon-offset capability
         * for the current primitive class is on) */
        if ((gradientMask & 1) != 0)
        {
            int d0 = v0->screenPosition[2];
            int d1 = v1->screenPosition[2];
            int d2 = v2->screenPosition[2];

            grSetupGradient(&info, GR_GRADIENT_DEPTH, d1 - d0, d2 - d0, d0);
            if (grIsDepthOffsetEnabled(state, state->primitiveType) != 0)
                grComputeDepthOffset(state);
        }

        /* fog gradient (fog rides the back-alpha slot, record +54) */
        if ((gradientMask & 2) != 0)
        {
            int f0 = (int)v0->color.final[1][3] << 8;
            int f1 = (int)v1->color.final[1][3] << 8;
            int f2 = (int)v2->color.final[1][3] << 8;

            grSetupGradient(&info, GR_GRADIENT_FOG, f1 - f0, f2 - f0,
                            f0 + 128);
        }

        /* color gradients: per-vertex when smooth shading is on, constant
         * from the flat-shade vertex otherwise */
        if ((gradientMask & 0x3C) != 0)
        {
            int side = tri->side;

            ok = 1;
            if (side != GR_SIDE_FRONT && side != GR_SIDE_BACK &&
                grAssert("side==GR_SIDE_FRONT || side==GR_SIDE_BACK",
                         "grRasterizer.inl", 749) != 0)
                ok = 0;
            if (ok == 0)
                grDie(0);
            if (grTestCapability(state, GR_CAPABILITY_SMOOTH_SHADING) != 0)
            {
                for (coord = GR_GRADIENT_COLOR_R; coord >= GR_GRADIENT_DEPTH; --coord)
                {
                    if (((4 << coord) & gradientMask) != 0)
                    {
                        int c0 = (int)v0->color.final[side][coord] << 8;
                        int c1 = (int)v1->color.final[side][coord] << 8;
                        int c2 = (int)v2->color.final[side][coord] << 8;

                        grSetupGradient(&info, coord + 2, c1 - c0, c2 - c0,
                                        c0 + 0x8000);
                    }
                }
                if ((gradientMask & 0x20) != 0)
                {
                    /* alpha always interpolates the FRONT alpha (+46) */
                    int a0 = (int)v0->color.final[0][3] << 8;
                    int a1 = (int)v1->color.final[0][3] << 8;
                    int a2 = (int)v2->color.final[0][3] << 8;

                    grSetupGradient(&info, GR_GRADIENT_COLOR_A,
                                    a1 - a0, a2 - a0, a0 + 0x8000);
                }
            }
            else
            {
                _grVertex *flat = vertexBuffer + flatShadeIndex;
                const unsigned short *flatRGB = &flat->color.final[side][0];

                for (coord = GR_GRADIENT_DEPTH; coord < GR_GRADIENT_COLOR_G; ++coord)
                {
                    /* reference[xAdder/yAdder at +192/+232/+272 raw] */
                    pixelPipe->reference[coord + 2] =
                        ((int)flatRGB[coord] << 8) + 0x8000;
                    pixelPipe->xAdder[coord + 2] = 0;
                    pixelPipe->yAdder[coord + 2] = 0;
                }
                pixelPipe->reference[GR_GRADIENT_COLOR_A] =
                    ((int)flat->color.final[0][3] << 8) + 0x8000;
                pixelPipe->xAdder[GR_GRADIENT_COLOR_A] = 0;
                pixelPipe->yAdder[GR_GRADIENT_COLOR_A] = 0;
            }
        }
    }
}

/* --- grDivsu (grScanConvert.inl 29; DLL 24922-24932) ------------------------- */

int grDivsu(int a, int b)
{
    if (b <= 0 && grAssert("b > 0", "grScanConvert.inl", 29) != 0)
        grDie(0);
    return a / b;
}

/* --- grScanConvertWithXScissoring (grScanConvert.inl 55; DLL 24935-25025) -----
 * Emits one u32 per scanline (left | numPixels << 16) for the top half and
 * then the bottom half, clamping the span to the scissor X range. The edge
 * x values are 18.14 (pixel << 14); the left edge is xStart[0], the right
 * edge xStart[1]. */

void grScanConvertWithXScissoring(_grEdgePair *edgePairs,
                                  unsigned int *scanlineInfo,
                                  _grRect *scissor, int topHeight,
                                  int btmHeight)
{
    int ok = (edgePairs != NULL && scanlineInfo != NULL && scissor != NULL);
    int x0 = scissor->x0;
    int x1 = scissor->x1;

    if (ok == 0 &&
        grAssert("edgePairs && scanlineInfo && scissor", "grScanConvert.inl", 55) != 0)
        grDie(0);
    if (topHeight != 0)
    {
        int leftX = edgePairs[0].xStart[0];
        int leftAdder = edgePairs[0].xAdder[0];
        int rightX = edgePairs[0].xStart[1];
        int rightAdder = edgePairs[0].xAdder[1];

        do
        {
            int left = hgMax32(leftX >> 14, x0);
            int right = hgMin32(rightX >> 14, x1);
            int numPixels = hgMaxZero32(right - left);

            *scanlineInfo++ = left + (numPixels << 16);
            if (hgInRange32(left, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(left, 0, 0xFFFF)", "grScanConvert.inl", 72) != 0)
                grDie(0);
            if (hgInRange32(numPixels, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(numPixels, 0, 0xFFFF)", "grScanConvert.inl", 73) != 0)
                grDie(0);
            leftX += leftAdder;
            rightX += rightAdder;
            --topHeight;
        }
        while (topHeight != 0);
    }
    if (btmHeight != 0)
    {
        int leftX = edgePairs[1].xStart[0];
        int leftAdder = edgePairs[1].xAdder[0];
        int rightX = edgePairs[1].xStart[1];
        int rightAdder = edgePairs[1].xAdder[1];

        do
        {
            int left = hgMax32(leftX >> 14, x0);
            int right = hgMin32(rightX >> 14, x1);
            int numPixels = hgMaxZero32(right - left);

            *scanlineInfo++ = left + (numPixels << 16);
            if (hgInRange32(left, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(left, 0, 0xFFFF)", "grScanConvert.inl", 95) != 0)
                grDie(0);
            if (hgInRange32(numPixels, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(numPixels, 0, 0xFFFF)", "grScanConvert.inl", 96) != 0)
                grDie(0);
            leftX += leftAdder;
            rightX += rightAdder;
            --btmHeight;
        }
        while (btmHeight != 0);
    }
}

/* --- grScanConvertWithoutXScissoring (grScanConvert.inl 118; DLL 25028-25111) - */

void grScanConvertWithoutXScissoring(_grEdgePair *edgePairs,
                                     unsigned int *scanlineInfo,
                                     int topHeight, int btmHeight)
{
    int ok = 1;

    if ((edgePairs == NULL || scanlineInfo == NULL) &&
        grAssert("edgePairs && scanlineInfo", "grScanConvert.inl", 118) != 0)
        ok = 0;
    if (ok == 0)
        grDie(0);
    if (topHeight != 0)
    {
        int leftX = edgePairs[0].xStart[0];
        int leftAdder = edgePairs[0].xAdder[0];
        int rightX = edgePairs[0].xStart[1];
        int rightAdder = edgePairs[0].xAdder[1];

        do
        {
            int left = leftX >> 14;
            int right = rightX >> 14;
            int numPixels = hgMaxZero32(right - left);

            *scanlineInfo++ = left + (numPixels << 16);
            if (hgInRange32(left, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(left, 0, 0xFFFF)", "grScanConvert.inl", 135) != 0)
                grDie(0);
            if (hgInRange32(numPixels, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(numPixels, 0, 0xFFFF)", "grScanConvert.inl", 136) != 0)
                grDie(0);
            leftX += leftAdder;
            rightX += rightAdder;
            --topHeight;
        }
        while (topHeight != 0);
    }
    if (btmHeight != 0)
    {
        int leftX = edgePairs[1].xStart[0];
        int leftAdder = edgePairs[1].xAdder[0];
        int rightX = edgePairs[1].xStart[1];
        int rightAdder = edgePairs[1].xAdder[1];

        do
        {
            int left = leftX >> 14;
            int right = rightX >> 14;
            int numPixels = hgMaxZero32(right - left);

            *scanlineInfo++ = left + (numPixels << 16);
            if (hgInRange32(left, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(left, 0, 0xFFFF)", "grScanConvert.inl", 158) != 0)
                grDie(0);
            if (hgInRange32(numPixels, 0, 0xFFFF) == 0 &&
                grAssert("hgInRange32(numPixels, 0, 0xFFFF)", "grScanConvert.inl", 159) != 0)
                grDie(0);
            leftX += leftAdder;
            rightX += rightAdder;
            --btmHeight;
        }
        while (btmHeight != 0);
    }
}

/* --- grScanConvertTriangle (grScanConvert.inl 249; DLL 25114-25331) -----------
 * p0/p1/p2 are y-sorted screen positions (20.12: pixel<<12, low emptyBits
 * bits zero). Splits the triangle at the middle vertex into a top half
 * (topHeight scanlines) and a bottom half (btmHeight), builds per-half
 * {left, right} edge pairs from the long edge (p0->p2) plus the two short
 * edges, y-clamps against the scissor, emits scanline words into the render
 * stack buffer at +15668, trims empty rows top/bottom and stores
 * {yStart, numScanlines, scanlineInfo} into the pixel pipe (+188/+180/+184).
 * Returns the final scanline count (0 when the triangle degenerates). */

int grScanConvertTriangle(Gerbera_s *state, _grRasterPoint *p0,
                          _grRasterPoint *p1, _grRasterPoint *p2,
                          int emptyBits)
{
    _grRect *scissor = &state->renderScissor;
    int d10x = (p1->x - p0->x) >> emptyBits;   /* 12.4 pixel deltas */
    int d10y = (p1->y - p0->y) >> emptyBits;
    int d20x = (p2->x - p0->x) >> emptyBits;
    int d20y = (p2->y - p0->y) >> emptyBits;
    int d21x = (p2->x - p1->x) >> emptyBits;
    int d21y = (p2->y - p1->y) >> emptyBits;
    int v0xT = grSubpixelTrunc(p0->x);         /* pixel-rounded vertices */
    int v0yT = grSubpixelTrunc(p0->y);
    int v1xT = grSubpixelTrunc(p1->x);
    int v1yT = grSubpixelTrunc(p1->y);
    int v2xT = grSubpixelTrunc(p2->x);
    int v2yT = grSubpixelTrunc(p2->y);
    int v0yTop = hgMin32(v0yT, scissor->y1);
    int v1yTop = hgMin32(v1yT, scissor->y1);
    int v2yTop = hgMin32(v2yT, scissor->y1);
    int v0yBottom = hgMax32(v0yT, scissor->y0);
    int topHeight = v1yTop - v0yTop;           /* scanlines above the middle */
    int btmHeight = v2yTop - v1yTop;           /* scanlines below the middle */
    int minX = hgMin32(hgMin32(v0xT, v1xT), v2xT);
    int maxX = hgMax32(hgMax32(v0xT, v1xT), v2xT);
    int needsXScissoring = minX < scissor->x0 || maxX > scissor->x1;
    int fracV0y = 4095 - (p0->y & 0xFFF);      /* distance to the next
                                                * scanline (0.12, -1 LSB) */
    int fracV1y = 4095 - (p1->y & 0xFFF);
    int slope02 = grDivsu(imLsl32OF(d20x, 14), d20y);  /* long-edge slope,
                                                * 2.14 per scanline */
    int longAtV0 = 4 * p0->x + (int)(((long long)fracV0y * slope02) >> 12);
    int longAtV1 = longAtV0 + slope02 * (v1yT - v0yT);  /* UNCLAMPED middle
                                                * delta (decompile p4.xAdder[1]
                                                * uses the raw truncs) */
    int longMid = longAtV1 - (((long long)fracV1y * slope02) >> 12);
    int side = (unsigned int)(4 * p1->x - longMid) >> 31;
    _grEdgePair edges[2];
    int yStart = v0yBottom;
    int total = btmHeight + topHeight;
    unsigned int *scanInfo;

    if (!(p0->y <= p1->y && p1->y <= p2->y) &&
        grAssert("v0sy <= v1sy && v1sy <= v2sy", "grScanConvert.inl", 249) != 0)
        grDie(0);
    if ((topHeight < 0 || btmHeight < 0) &&
        grAssert("topHeight >= 0 && btmHeight >= 0", "grScanConvert.inl", 250) != 0)
        grDie(0);
    if (((1 << emptyBits) - 1) & p0->x &&
        grAssert("!(v0sx & ((1<<emptyBits)-1))", "grScanConvert.inl", 252) != 0)
        grDie(0);
    if (((1 << emptyBits) - 1) & p0->y &&
        grAssert("!(v0sy & ((1<<emptyBits)-1))", "grScanConvert.inl", 253) != 0)
        grDie(0);
    if (((1 << emptyBits) - 1) & p1->x &&
        grAssert("!(v1sx & ((1<<emptyBits)-1))", "grScanConvert.inl", 254) != 0)
        grDie(0);
    if (((1 << emptyBits) - 1) & p1->y &&
        grAssert("!(v1sy & ((1<<emptyBits)-1))", "grScanConvert.inl", 255) != 0)
        grDie(0);
    if (((1 << emptyBits) - 1) & p2->x &&
        grAssert("!(v2sx & ((1<<emptyBits)-1))", "grScanConvert.inl", 256) != 0)
        grDie(0);
    if (((1 << emptyBits) - 1) & p2->y &&
        grAssert("!(v2sy & ((1<<emptyBits)-1))", "grScanConvert.inl", 257) != 0)
        grDie(0);

    memset(edges, 0xCC, sizeof(edges));
    edges[0].xStart[side] = (int)longAtV0;     /* long edge, top half */
    edges[0].xAdder[side] = slope02;
    edges[1].xStart[side] = longAtV1;          /* long edge, bottom half
                                                * (continues at v1) */
    edges[1].xAdder[side] = slope02;
    side ^= 1;
    if (topHeight > 0)
    {
        int slope01 = grDivsu(imLsl32OF(d10x, 14), d10y);

        edges[0].xStart[side] =
            4 * p0->x + (int)(((long long)fracV0y * slope01) >> 12);
        edges[0].xAdder[side] = slope01;
    }
    if (btmHeight > 0)
    {
        int slope12 = grDivsu(imLsl32OF(d21x, 14), d21y);

        edges[1].xStart[side] =
            4 * p1->x + (int)(((long long)fracV1y * slope12) >> 12);
        edges[1].xAdder[side] = slope12;
    }

    /* the top vertex above the scissor: advance both top edges and shrink
     * the top half; the middle vertex above it: advance both bottom edges */
    if (v0yTop < scissor->y0)
    {
        int advance = scissor->y0 - v0yTop;

        topHeight = hgMaxZero32(topHeight - advance);
        edges[0].xStart[0] += edges[0].xAdder[0] * advance;
        edges[0].xStart[1] += edges[0].xAdder[1] * advance;
        if (v1yTop < scissor->y0)
        {
            int advanceMid = scissor->y0 - v1yTop;

            btmHeight = hgMaxZero32(btmHeight - advanceMid);
            edges[1].xStart[0] += edges[1].xAdder[0] * advanceMid;
            edges[1].xStart[1] += edges[1].xAdder[1] * advanceMid;
        }
    }
    total = btmHeight + topHeight;
    if (total != 0)
    {
        scanInfo = (unsigned int *)((unsigned char *)state->renderStack +
                                    GR_RENDER_STACK_SCANLINE_INFO);
        if (needsXScissoring)
            grScanConvertWithXScissoring(edges, scanInfo, scissor,
                                         topHeight, btmHeight);
        else
            grScanConvertWithoutXScissoring(edges, scanInfo,
                                            topHeight, btmHeight);
        while (total != 0 &&
               (int)(scanInfo[total - 1] >> 16) == 0)
            --total;
        while (total != 0 && (int)(*scanInfo >> 16) == 0)
        {
            --total;
            ++scanInfo;
            ++yStart;
        }
        if (yStart + total > scissor->y1 &&
            grAssert("yStart + numScanlines <= scissor->y1",
                     "grScanConvert.inl", 359) != 0)
            grDie(0);
        if (hgInRange32(yStart, scissor->y0, scissor->y1) == 0 &&
            grAssert("hgInRange32(yStart, scissor->y0, scissor->y1)",
                     "grScanConvert.inl", 360) != 0)
            grDie(0);
        if (hgInRange32(total, 0, GR_MAX_DISPLAY_DIMENSION) == 0 &&
            grAssert("hgInRange32(numScanlines, 0, GR_MAX_DISPLAY_DIMENSION)",
                     "grScanConvert.inl", 361) != 0)
            grDie(0);
        _grPixelPipe *pipe = (_grPixelPipe *)state->pixelPipeline;

        pipe->yStart = yStart;
        pipe->numScanlines = total;
        pipe->scanlineInfo = scanInfo;
    }
    return total;
}
