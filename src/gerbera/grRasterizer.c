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
