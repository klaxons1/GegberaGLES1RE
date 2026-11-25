/* grCcsoft.c - CCsoft-style specialised C pixel spans (GERBERA_CCSOFT).
 *
 * See grCcsoft.h. grCcsoftClassify runs when the pixel pipeline is
 * revalidated and picks the spans for the current state class;
 * grCcsoftExecute runs per primitive in place of the JIT program. It reads
 * the same pixel-pipe record the JIT reads (scanline list, row pointers,
 * gradient references and adders, texture unit 0) and follows the JIT's
 * arithmetic, as read from its generated ARM code. For in-range values that
 * equals grPixelPipeline.c's clamping grExecutePixelPipeline64:
 *
 *   colour    (unsigned)cur >> 16, unclamped, or the current colour when
 *             the gradient is not active (min((rgba16 + 128) >> 8, 255))
 *   depth     (unsigned)cur >> 14 against a Z16 buffer, unclamped (the JIT
 *             clamps only for polygon offset, which is left to the JIT)
 *   texel     repeat: ((unsigned)cur >> rShift) & mask, nearest sample,
 *             565 and 8888 widened by bit replication
 *   MODULATE  (p * t + p) >> 8, saturated to 255 for smooth colour
 *             (REPLACE is MODULATE by white)
 *   fog       ((256 - f) * c + f * fogColor) >> 8, f unclamped
 *   blend     min(((s * (sw + 1)) >> 8) + ((d * (dw + 1)) >> 8), 255);
 *             ONE/ONE is min(s + d, 255)
 *   dither    c - (c >> bits) + (k >> bits), k from g_orderedDitherKernel
 *   pack      (r >> 3) << 11 | (g >> 2) << 5 | b >> 3, truncated to 16 bits
 *
 * Every span is compiled for one state class (texture kind, shading, depth
 * test, blend kind, fog, alpha test, dither), so it is a short loop over
 * locals; a white 565 texture with nothing else on is a texel copy, as in
 * CCsoftgl. Only the classes in CC_SPAN_LIST are built; other states use the
 * JIT.
 */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "gerbera/grCcsoft.h"

#if defined(GERBERA_CCSOFT)

#if defined(GERBERA_CCSOFT_TOGGLE)
int g_grCcsoftEnabled = 1;
unsigned int g_grCcsoftStats[2];
#endif

extern const int g_orderedDitherKernel[4];

/* The spans are already specialised; loop unswitching on the remaining
 * run-time tests only multiplies their size. */
#pragma GCC push_options
#pragma GCC optimize("no-unswitch-loops", "no-predictive-commoning")

enum { CC_TEX_NONE = 0, CC_TEX_565 = 1, CC_TEX_8888 = 2 };            /* 8888 also RGB8 */
enum { CC_SHADE_WHITE = 0, CC_SHADE_FLAT = 1, CC_SHADE_SMOOTH = 2 };
enum { CC_BLEND_NONE = 0, CC_BLEND_ALPHA = 1, CC_BLEND_MULTIPLY = 2, CC_BLEND_ADD = 3 }; /* ALPHA: weights linear in source alpha */

/* class key without the shading, which is chosen per primitive */
#define CC_KEY(TEX, DEPTH, BLEND, FOG, ATEST, DITHER) \
    ((TEX) | ((DEPTH) << 2) | ((BLEND) << 3) | ((FOG) << 5) | ((ATEST) << 6) | ((DITHER) << 7))
#define CC_KEY_VALID 0x100u

/* per-primitive values; per-state ones are in Gerbera_s.ccsoft */
typedef struct
{
    const _grPixelPipe *pp;
    int colorStride, depthStride;
    int r, g, b, a;             /* constant colour */
    const void *texels;
    int uShift, vShift;         /* s/t pre-shift so the index bits end at bit 31 */
    int uRight, vRight;         /* 32 - log2 size */
    int log2S;
    unsigned int alphaOr;       /* RGB8 textures: 0xFF000000 */
    int alphaLow;
    unsigned int alphaRange;    /* passes when (unsigned)(a - alphaLow) <= range */
} CcSpan;

typedef void (*CcSpanFn)(const grCcsoftState *st, const CcSpan *c);

static inline __attribute__((always_inline)) int ccClamp255(int v)
{
    if ((unsigned int)v > 255u)
        v = v < 0 ? 0 : 255;
    return v;
}

/* the JIT's 16-bit depth: unclamped (it clamps only for polygon offset,
 * which classify leaves to the JIT); compared in full against the buffer */
static inline unsigned int ccDepth16(unsigned int z)
{
    return z >> 14;
}

static inline __attribute__((always_inline))
void ccSpanBody(const grCcsoftState *st, const CcSpan *cp, const int TEX, const int SHADE, const int DEPTH,
                const int BLEND, const int FOG, const int ATEST, const int DITHER)
{
    const int needAlpha = BLEND == CC_BLEND_ALPHA || ATEST;
    const int copy = TEX == CC_TEX_565 && SHADE == CC_SHADE_WHITE && !BLEND && !FOG && !ATEST && !DITHER;
    const int pack8888 = TEX == CC_TEX_8888 && SHADE == CC_SHADE_WHITE && !BLEND && !FOG && !ATEST && !DITHER;
    const _grPixelPipe *pp = cp->pp;
    const unsigned int *scan = pp->scanlineInfo;
    const int lines = pp->numScanlines;
    unsigned char *colorRow = (unsigned char *)pp->colorRow;
    unsigned char *depthRow = (unsigned char *)pp->depthRow;
    const int colorStride = cp->colorStride, depthStride = cp->depthStride;
    const int yStart = pp->yStart;
    const int cr = cp->r, cg = cp->g, cb = cp->b, ca = cp->a;
    const void *const texels = cp->texels;
    const int uShift = cp->uShift, vShift = cp->vShift;
    const int uRight = cp->uRight, vRight = cp->vRight, log2S = cp->log2S;
    const unsigned int alphaOr = cp->alphaOr;
    const int fogR = st->fogR, fogG = st->fogG, fogB = st->fogB;
    const int alphaLow = cp->alphaLow;
    const unsigned int alphaRange = cp->alphaRange;
    const int depthLess = st->depthLess, depthEqual = st->depthEqual, depthGreater = st->depthGreater;
    const int depthWrite = st->depthWrite;
    const int srcA = st->srcA, srcB = st->srcB, dstA = st->dstA, dstB = st->dstB;
    /* interpolants in 32-bit wrapping arithmetic, as the JIT's */
    unsigned int zRef = 0, zDx = 0, zDy = 0, fRef = 0, fDx = 0, fDy = 0;
    unsigned int rRef = 0, rDx = 0, rDy = 0, gRef = 0, gDx = 0, gDy = 0;
    unsigned int bRef = 0, bDx = 0, bDy = 0, aRef = 0, aDx = 0, aDy = 0;
    unsigned int uRef = 0, uDx = 0, uDy = 0, vRef = 0, vDx = 0, vDy = 0;
    int line;

    /* the class's gradients, straight from the pixel pipe */
    const int *const gRefs = pp->reference, *const gDxs = pp->xAdder, *const gDys = pp->yAdder;
#define CC_GRAD(G, R, X, Y) \
    (R = (unsigned int)gRefs[G], X = (unsigned int)gDxs[G], Y = (unsigned int)gDys[G])

    if (DEPTH) CC_GRAD(0, zRef, zDx, zDy);
    if (FOG) CC_GRAD(1, fRef, fDx, fDy);
    if (SHADE == CC_SHADE_SMOOTH)
    {
        CC_GRAD(2, rRef, rDx, rDy);
        CC_GRAD(3, gRef, gDx, gDy);
        CC_GRAD(4, bRef, bDx, bDy);
        if (needAlpha) CC_GRAD(5, aRef, aDx, aDy);
    }
    if (TEX)
    {
        CC_GRAD(6, uRef, uDx, uDy);
        CC_GRAD(7, vRef, vDx, vDy);
        uRef <<= uShift; uDx <<= uShift; uDy <<= uShift;
        vRef <<= vShift; vDx <<= vShift; vDy <<= vShift;
#undef CC_GRAD
    }

    for (line = 0; line < lines; ++line)
    {
        const unsigned int info = scan[line];
        const unsigned int x0 = info & 0xFFFFu;
        int n = (int)(info >> 16);
        unsigned short *dst = (unsigned short *)colorRow + x0;
        unsigned short *zb = (unsigned short *)depthRow + x0;
        unsigned int z = 0, f = 0, r = 0, g = 0, b = 0, a = 0, u = 0, v = 0, kw = 0;

        if (DEPTH) z = zRef + zDx * x0;
        if (FOG) f = fRef + fDx * x0;
        if (SHADE == CC_SHADE_SMOOTH)
        {
            r = rRef + rDx * x0;
            g = gRef + gDx * x0;
            b = bRef + bDx * x0;
            if (needAlpha) a = aRef + aDx * x0;
        }
        if (TEX)
        {
            u = uRef + uDx * x0;
            v = vRef + vDx * x0;
        }
        if (DITHER)
        {
            /* grDitherColor: k = ror(row, 8 * (x & 3)) >> 24; rotates right
             * by 8 per pixel */
            const unsigned int row = (unsigned int)g_orderedDitherKernel[(yStart + line) & 3];
            const unsigned int left = 8u * ((0u - x0) & 3u);
            kw = left ? (row << left) | (row >> (32u - left)) : row;
        }

        for (; n > 0; --n, ++dst, ++zb)
        {
            unsigned int zv = 0;
            int pr, pg, pb, pa;

            if (DEPTH && !ATEST)
            {
                const unsigned int zd = *zb;
                zv = ccDepth16(z);
                if (!(zv < zd ? depthLess : (zv == zd ? depthEqual : depthGreater)))
                    goto step;
                if (depthWrite)
                    *zb = (unsigned short)zv;
            }

            if (copy)
            {
                *dst = ((const unsigned short *)texels)[((v >> vRight) << log2S) + (u >> uRight)];
                goto step;
            }
            if (pack8888)
            {
                const unsigned int t = ((const unsigned int *)texels)[((v >> vRight) << log2S) + (u >> uRight)];
                *dst = (unsigned short)(((t >> 8) & 0xF800u) | ((t >> 5) & 0x07E0u) | ((t >> 3) & 0x001Fu));
                goto step;
            }

            if (SHADE == CC_SHADE_SMOOTH)
            {
                /* the JIT reads colour interpolants unclamped */
                pr = (int)(r >> 16);
                pg = (int)(g >> 16);
                pb = (int)(b >> 16);
                pa = needAlpha ? (int)(a >> 16) : 255;
            }
            else
            {
                pr = cr;
                pg = cg;
                pb = cb;
                pa = ca;
            }

            if (TEX)
            {
                const unsigned int index = ((v >> vRight) << log2S) + (u >> uRight);
                int tr, tg, tb;
                if (TEX == CC_TEX_565)
                {
                    const unsigned int t = ((const unsigned short *)texels)[index];
                    tr = (int)(t >> 11);
                    tg = (int)((t >> 5) & 63u);
                    tb = (int)(t & 31u);
                    tr = (tr << 3) | (tr >> 2);
                    tg = (tg << 2) | (tg >> 4);
                    tb = (tb << 3) | (tb >> 2);
                }
                else
                {
                    const unsigned int t = ((const unsigned int *)texels)[index];
                    tr = (int)((t >> 16) & 255u);
                    tg = (int)((t >> 8) & 255u);
                    tb = (int)(t & 255u);
                    if (needAlpha)
                    {
                        const int ta = (int)((t | alphaOr) >> 24);
                        pa = (pa * ta + pa) >> 8;
                    }
                    if (needAlpha && SHADE == CC_SHADE_SMOOTH && pa > 255)
                        pa = 255;
                }
                if (SHADE == CC_SHADE_WHITE)
                {
                    pr = tr;
                    pg = tg;
                    pb = tb;
                }
                else
                {
                    pr = (pr * tr + pr) >> 8;
                    pg = (pg * tg + pg) >> 8;
                    pb = (pb * tb + pb) >> 8;
                    if (SHADE == CC_SHADE_SMOOTH)
                    {
                        /* the JIT saturates the modulated colour */
                        if (pr > 255) pr = 255;
                        if (pg > 255) pg = 255;
                        if (pb > 255) pb = 255;
                        if (TEX == CC_TEX_565 && needAlpha && pa > 255)
                            pa = 255;
                    }
                }
            }

            if (FOG)
            {
                const int fv = (int)(f >> 16);   /* unclamped, as the JIT */
                const int inv = 256 - fv;
                pr = (inv * pr + fv * fogR) >> 8;
                pg = (inv * pg + fv * fogG) >> 8;
                pb = (inv * pb + fv * fogB) >> 8;
            }

            if (ATEST && (unsigned int)(pa - alphaLow) > alphaRange)
                goto step;

            if (DEPTH && ATEST)
            {
                const unsigned int zd = *zb;
                zv = ccDepth16(z);
                if (!(zv < zd ? depthLess : (zv == zd ? depthEqual : depthGreater)))
                    goto step;
                if (depthWrite)
                    *zb = (unsigned short)zv;
            }

            if (BLEND)
            {
                const unsigned int d = *dst;
                int dr = (int)(d >> 11), dg = (int)((d >> 5) & 63u), db = (int)(d & 31u);
                dr = (dr << 3) | (dr >> 2);
                dg = (dg << 2) | (dg >> 4);
                db = (db << 3) | (db >> 2);
                if (BLEND == CC_BLEND_ADD)
                {
                    /* ONE, ONE */
                    pr += dr;
                    pg += dg;
                    pb += db;
                    if (pr > 255) pr = 255;
                    if (pg > 255) pg = 255;
                    if (pb > 255) pb = 255;
                }
                else if (BLEND == CC_BLEND_MULTIPLY)
                {
                    /* DST_COLOR, ZERO: (s * (d + 1)) >> 8 + (d * 1) >> 8 */
                    pr = (pr * (dr + 1)) >> 8;
                    pg = (pg * (dg + 1)) >> 8;
                    pb = (pb * (db + 1)) >> 8;
                }
                else
                {
                    const int sw = srcA + srcB * pa, dw = dstA + dstB * pa;
                    pr = ((pr * sw) >> 8) + ((dr * dw) >> 8);
                    pg = ((pg * sw) >> 8) + ((dg * dw) >> 8);
                    pb = ((pb * sw) >> 8) + ((db * dw) >> 8);
                    if (pr > 255) pr = 255;
                    if (pg > 255) pg = 255;
                    if (pb > 255) pb = 255;
                }
            }

            if (DITHER)
            {
                const int k5 = (int)(kw >> 29), k6 = (int)(kw >> 30);
                pr = pr - (pr >> 5) + k5;
                pg = pg - (pg >> 6) + k6;
                pb = pb - (pb >> 5) + k5;
            }

            /* the JIT's unmasked pack; equal to the masked one for 0..255 */
            *dst = (unsigned short)((((unsigned int)pr >> 3) << 11) | (((unsigned int)pg >> 2) << 5) |
                                    ((unsigned int)pb >> 3));

step:
            if (DEPTH) z += zDx;
            if (FOG) f += fDx;
            if (SHADE == CC_SHADE_SMOOTH)
            {
                r += rDx;
                g += gDx;
                b += bDx;
                if (needAlpha) a += aDx;
            }
            if (TEX)
            {
                u += uDx;
                v += vDx;
            }
            if (DITHER)
                kw = (kw >> 8) | (kw << 24);
        }

        if (DEPTH) zRef += zDy;
        if (FOG) fRef += fDy;
        if (SHADE == CC_SHADE_SMOOTH)
        {
            rRef += rDy;
            gRef += gDy;
            bRef += bDy;
            if (needAlpha) aRef += aDy;
        }
        if (TEX)
        {
            uRef += uDy;
            vRef += vDy;
        }
        colorRow += colorStride;
        depthRow += depthStride;
    }
}

/* --- the built state classes ------------------------------------------------------
 * M(TEX, SHADE, DEPTH, BLEND, FOG, ATEST, DITHER) */

#define CC_DD(M, T, S, B, F, A) \
    M(T, S, 0, B, F, A, 0) M(T, S, 0, B, F, A, 1) M(T, S, 1, B, F, A, 0) M(T, S, 1, B, F, A, 1)
/* every texture/shading pair; no texture has no white (it is flat) */
#define CC_TS_ALL(M, B, F, A) \
    CC_DD(M, 0, 1, B, F, A) CC_DD(M, 0, 2, B, F, A) \
    CC_DD(M, 1, 0, B, F, A) CC_DD(M, 1, 1, B, F, A) CC_DD(M, 1, 2, B, F, A) \
    CC_DD(M, 2, 0, B, F, A) CC_DD(M, 2, 1, B, F, A) CC_DD(M, 2, 2, B, F, A)
/* blending: white modulate is flat modulate (no copy shortcut) */
#define CC_TS_BLEND(M, B) \
    CC_DD(M, 0, 1, B, 0, 0) CC_DD(M, 0, 2, B, 0, 0) \
    CC_DD(M, 1, 1, B, 0, 0) CC_DD(M, 1, 2, B, 0, 0) \
    CC_DD(M, 2, 0, B, 0, 0) CC_DD(M, 2, 1, B, 0, 0) CC_DD(M, 2, 2, B, 0, 0)
/* fog: depth tested scenes */
#define CC_FOG_D(M, T, S) M(T, S, 1, 0, 1, 0, 0) M(T, S, 1, 0, 1, 0, 1)
/* alpha test: RGBA textures, opaque or alpha blended */
#define CC_ATEST(M, B) CC_DD(M, 2, 0, B, 0, 1) CC_DD(M, 2, 1, B, 0, 1) CC_DD(M, 2, 2, B, 0, 1)

#define CC_SPAN_LIST(M) \
    CC_TS_ALL(M, 0, 0, 0) \
    CC_TS_BLEND(M, 1) \
    CC_TS_BLEND(M, 2) \
    CC_TS_BLEND(M, 3) \
    CC_FOG_D(M, 0, 1) CC_FOG_D(M, 0, 2) CC_FOG_D(M, 1, 0) CC_FOG_D(M, 1, 1) CC_FOG_D(M, 1, 2) \
    CC_FOG_D(M, 2, 0) CC_FOG_D(M, 2, 1) CC_FOG_D(M, 2, 2) \
    CC_ATEST(M, 0) CC_ATEST(M, 1)

#define CC_DEFINE_SPAN(T, S, D, B, F, A, K) \
    static void ccSpan_##T##S##D##B##F##A##K(const grCcsoftState *st, const CcSpan *c) \
    { ccSpanBody(st, c, T, S, D, B, F, A, K); }
CC_SPAN_LIST(CC_DEFINE_SPAN)

typedef struct
{
    unsigned short key;    /* CC_KEY | shading << 9 */
    CcSpanFn span;
} CcSpanEntry;

#define CC_SPAN_ENTRY(T, S, D, B, F, A, K) \
    { (unsigned short)(CC_KEY(T, D, B, F, A, K) | ((S) << 9)), ccSpan_##T##S##D##B##F##A##K },
static const CcSpanEntry s_ccSpans[] = { CC_SPAN_LIST(CC_SPAN_ENTRY) };

static CcSpanFn ccFindSpan(unsigned int key, int shade)
{
    const unsigned short wanted = (unsigned short)(key | ((unsigned int)shade << 9));
    unsigned int i;
    for (i = 0; i < sizeof(s_ccSpans) / sizeof(s_ccSpans[0]); ++i)
        if (s_ccSpans[i].key == wanted)
            return s_ccSpans[i].span;
    return NULL;
}

#pragma GCC pop_options

/* --- classification ---------------------------------------------------------------- */

static int ccIs565(const _grDrawSurface *surf)
{
    return surf->colorBytes == 2 &&
           surf->rgbaSizes[0] == 5 && surf->rgbaSizes[1] == 6 &&
           surf->rgbaSizes[2] == 5 && surf->rgbaSizes[3] == 0 &&
           surf->rgbaOffsets[0] == 11 && surf->rgbaOffsets[1] == 5 && surf->rgbaOffsets[2] == 0;
}

/* weight + 1 as A + B * source alpha against a 565 buffer (destination alpha
 * reads 255); colour-dependent factors return 0 */
static int ccAlphaWeight(int factor, int *a, int *b)
{
    switch (factor)
    {
    case GR_BLENDFACTOR_ZERO:                *a = 1;   *b = 0;  return 1;
    case GR_BLENDFACTOR_ONE:                 *a = 256; *b = 0;  return 1;
    case GR_BLENDFACTOR_SRC_ALPHA:           *a = 1;   *b = 1;  return 1;
    case GR_BLENDFACTOR_ONE_MINUS_SRC_ALPHA: *a = 256; *b = -1; return 1;
    case GR_BLENDFACTOR_DST_ALPHA:           *a = 256; *b = 0;  return 1;
    case GR_BLENDFACTOR_ONE_MINUS_DST_ALPHA: *a = 1;   *b = 0;  return 1;
    default: return 0;
    }
}

void grCcsoftClassify(Gerbera_s *state)
{
    const _grDrawSurface *surf = &state->drawSurface;
    grCcsoftState *st = &state->ccsoft;
    const int gm = state->activeGradientMask;
    int tex = CC_TEX_NONE, depth = 0, blend = CC_BLEND_NONE, fog = 0, atest = 0, dither = 0;
    int texActive, i, f;
    unsigned int key;

    st->key = 0;
    if (!ccIs565(surf) || surf->buffers[0].data == NULL)
        return;
    if ((state->colorMask & surf->colorMask) != surf->colorMask || surf->colorMask == 0)
        return;
    if (grTestCapability(state, GR_CAPABILITY_COLOR_LOGIC_OP))
        return;
    if (grTestCapability(state, GR_CAPABILITY_STENCIL_TEST) && surf->stencilBytes != 0)
        return;
    dither = state->useDithering != 0;

    if (grTestCapability(state, GR_CAPABILITY_DEPTH_TEST) && surf->depthBytes != 0)
    {
        if (surf->depthBytes != 2 || !(gm & 1))
            return;
        if (grTestCapability(state, GR_CAPABILITY_DEPTH_OFFSET_POINT) ||
            grTestCapability(state, GR_CAPABILITY_DEPTH_OFFSET_LINE) ||
            grTestCapability(state, GR_CAPABILITY_DEPTH_OFFSET_FILL))
            return;   /* the JIT clamps the offset depth */
        depth = 1;
    }
    f = state->depthFunc;
    st->depthLess = f == GR_FUNC_LESS || f == GR_FUNC_LEQUAL || f == GR_FUNC_NOTEQUAL || f == GR_FUNC_ALWAYS;
    st->depthEqual = f == GR_FUNC_EQUAL || f == GR_FUNC_LEQUAL || f == GR_FUNC_GEQUAL || f == GR_FUNC_ALWAYS;
    st->depthGreater = f == GR_FUNC_GREATER || f == GR_FUNC_NOTEQUAL || f == GR_FUNC_GEQUAL || f == GR_FUNC_ALWAYS;
    st->depthWrite = state->depthMask != 0;

    if (grTestCapability(state, GR_CAPABILITY_ALPHA_TEST))
    {
        if (state->alphaFunc == GR_FUNC_NEVER || state->alphaFunc == GR_FUNC_NOTEQUAL)
            return;
        atest = state->alphaFunc != GR_FUNC_ALWAYS;
    }

    st->srcA = st->srcB = st->dstA = st->dstB = 0;
    if (grTestCapability(state, GR_CAPABILITY_BLEND) &&
        (state->srcBlendFactor != GR_BLENDFACTOR_ONE || state->dstBlendFactor != GR_BLENDFACTOR_ZERO))
    {
        if (state->srcBlendFactor == GR_BLENDFACTOR_ONE && state->dstBlendFactor == GR_BLENDFACTOR_ONE)
            blend = CC_BLEND_ADD;
        else if (state->srcBlendFactor == GR_BLENDFACTOR_DST_COLOR && state->dstBlendFactor == GR_BLENDFACTOR_ZERO)
            blend = CC_BLEND_MULTIPLY;
        else if (ccAlphaWeight(state->srcBlendFactor, &st->srcA, &st->srcB) &&
                 ccAlphaWeight(state->dstBlendFactor, &st->dstA, &st->dstB))
            blend = CC_BLEND_ALPHA;
        else
            return;
    }

    fog = grTestCapability(state, GR_CAPABILITY_FOG) != 0;
    if (fog && !(gm & 2))
        return;   /* constant fog: the span reads the fog gradient */
    st->fogR = (int)(state->fogColor & 0xFFu);
    st->fogG = (int)((state->fogColor >> 8) & 0xFFu);
    st->fogB = (int)((state->fogColor >> 16) & 0xFFu);

    /* unit 1 must be idle; unit 0 a nearest, repeat RGB565/RGB8/RGBA8 texture */
    if ((state->useTexUnit[1] || state->textureUnitEnabled[1]) && state->textureUnit[2].texture != NULL)
        return;
    texActive = (state->useTexUnit[0] || state->textureUnitEnabled[0]) && state->textureUnit[0].texture != NULL;
    st->replace = 0;
    st->textureFormat = -1;
    if (texActive)
    {
        const _grTexture *t = state->textureUnit[0].texture;
        const int env = state->textureUnit[0].texEnvMode;
        if (t->wrapMode[0] != GR_WRAPMODE_REPEAT || t->wrapMode[1] != GR_WRAPMODE_REPEAT)
            return;
        if (env != GR_TEXENVMODE_MODULATE && env != GR_TEXENVMODE_REPLACE)
            return;
        if ((gm & 0xC0) != 0xC0)
            return;
        st->replace = env == GR_TEXENVMODE_REPLACE;
        st->textureFormat = t->mipmap[0].texFormat;
        switch (st->textureFormat)
        {
        case GR_TEXTUREFORMAT_RGB565: tex = CC_TEX_565; break;
        case GR_TEXTUREFORMAT_RGBA8:
        case GR_TEXTUREFORMAT_RGB8:   tex = CC_TEX_8888; break;
        default: return;
        }
    }

    key = CC_KEY(tex, depth, blend, fog, atest, dither);
    for (i = 0; i < 3; ++i)
        st->span[i] = (void *)ccFindSpan(key, i);
    if (st->span[0] == NULL && st->span[1] == NULL && st->span[2] == NULL)
        return;
    st->gradientMask = gm;
    st->tex = tex;
    st->needAlpha = blend == CC_BLEND_ALPHA || atest;
    st->colorUse = gm & (st->needAlpha ? 0x3C : 0x1C);
    st->smoothOk = st->colorUse == (st->needAlpha ? 0x3C : 0x1C);
    st->key = key | CC_KEY_VALID;
}

/* --- execution ------------------------------------------------------------------- */

static inline int ccDefaultColor(const Gerbera_s *state, int c)
{
    const int v = ((int)state->defaultColorRGBA[c] + 128) >> 8;   /* 0..256 */
    return v > 255 ? 255 : v;
}

/* log2(mask + 1) for masks 2^n - 1 with n >= 1, else -1 */
static inline int ccLog2Mask(unsigned int mask)
{
    if (mask == 0 || (mask & (mask + 1u)) != 0)
        return -1;
    return 32 - __builtin_clz(mask);
}


int grCcsoftExecute(Gerbera_s *state)
{
    const grCcsoftState *st = &state->ccsoft;
    const unsigned int key = st->key;
    const _grPixelPipe *pp;
    int gm, tex, needAlpha, shade;
    CcSpanFn span;
    CcSpan c;

    if (!(key & CC_KEY_VALID) || !GR_CCSOFT_ENABLED)
        goto jit;
    pp = (const _grPixelPipe *)state->pixelPipeline;
    if (pp->numScanlines <= 0)
        return 1;
    gm = st->gradientMask;
    tex = st->tex;
    needAlpha = st->needAlpha;
    c.pp = pp;
    c.colorStride = state->drawSurface.buffers[0].stride;
    c.depthStride = state->drawSurface.buffers[1].stride;
    if (key & CC_KEY(0, 1, 0, 0, 0, 0))
    {
        if (pp->depthRow == NULL)
            goto jit;
    }

    /* colour: constant over the primitive unless a used gradient moves */
    if (tex != CC_TEX_NONE && st->replace)
    {
        /* REPLACE is MODULATE by white; RGBA textures replace alpha too */
        const int rgba = st->textureFormat == GR_TEXTUREFORMAT_RGBA8;
        if (needAlpha && !rgba && (gm & 0x20) && (pp->xAdder[5] | pp->yAdder[5]) != 0)
            goto jit;
        c.r = c.g = c.b = 255;
        c.a = rgba ? 255 : ((gm & 0x20) ? ccClamp255(pp->reference[5] >> 16) : ccDefaultColor(state, 3));
        shade = CC_SHADE_WHITE;
    }
    else
    {
        const int use = st->colorUse;
        const int *xa = pp->xAdder, *ya = pp->yAdder;
        int moving = 0;
        if (use)
        {
            if (use & 0x04) moving |= xa[2] | ya[2];
            if (use & 0x08) moving |= xa[3] | ya[3];
            if (use & 0x10) moving |= xa[4] | ya[4];
            if (use & 0x20) moving |= xa[5] | ya[5];
        }
        if (!moving)
        {
            c.r = (gm & 0x04) ? ccClamp255(pp->reference[2] >> 16) : ccDefaultColor(state, 0);
            c.g = (gm & 0x08) ? ccClamp255(pp->reference[3] >> 16) : ccDefaultColor(state, 1);
            c.b = (gm & 0x10) ? ccClamp255(pp->reference[4] >> 16) : ccDefaultColor(state, 2);
            c.a = (gm & 0x20) ? ccClamp255(pp->reference[5] >> 16) : ccDefaultColor(state, 3);
            shade = (tex != CC_TEX_NONE && (c.r & c.g & c.b) == 255) ? CC_SHADE_WHITE : CC_SHADE_FLAT;
        }
        else
        {
            /* the span reads the colour gradients; a missing one (a
             * constant channel among moving ones) is left to the JIT */
            if (!st->smoothOk)
                goto jit;
            shade = CC_SHADE_SMOOTH;
            c.r = c.g = c.b = c.a = 255;
        }
    }
    span = (CcSpanFn)st->span[shade];
    if (span == NULL && shade == CC_SHADE_WHITE)
        span = (CcSpanFn)st->span[shade = CC_SHADE_FLAT];
    if (span == NULL)
        goto jit;

    c.texels = NULL;
    c.uShift = c.vShift = c.uRight = c.vRight = c.log2S = 0;
    c.alphaOr = 0;
    if (tex != CC_TEX_NONE)
    {
        const _grPixelPipeTextureUnit *tu = &pp->textureUnit[0];
        const int log2S = tu->log2DimS;
        const int log2T = ccLog2Mask((unsigned int)tu->tMask);
        const int sShift = tu->rShift[0], tShift = tu->rShift[1];
        if (tu->pixels == NULL || tu->linear || log2T < 0 ||
            ccLog2Mask((unsigned int)tu->sMask) != log2S ||
            (unsigned int)sShift > (unsigned int)(32 - log2S) ||
            (unsigned int)tShift > (unsigned int)(32 - log2T))
            goto jit;
        c.texels = tu->pixels;
        c.uShift = 32 - sShift - log2S;
        c.vShift = 32 - tShift - log2T;
        c.uRight = 32 - log2S;
        c.vRight = 32 - log2T;
        c.log2S = log2S;
        if (st->textureFormat == GR_TEXTUREFORMAT_RGB8)
            c.alphaOr = 0xFF000000u;

    }

    c.alphaLow = 0;
    c.alphaRange = 255;
    if (key & CC_KEY(0, 0, 0, 0, 1, 0))
    {
        const int ref = state->alphaReference;
        switch (state->alphaFunc)
        {
        case GR_FUNC_LESS:    if (ref <= 0) goto jit; c.alphaRange = (unsigned int)(ref - 1); break;
        case GR_FUNC_EQUAL:   c.alphaLow = ref; c.alphaRange = 0; break;
        case GR_FUNC_LEQUAL:  c.alphaRange = (unsigned int)ref; break;
        case GR_FUNC_GREATER: if (ref >= 255) goto jit; c.alphaLow = ref + 1; c.alphaRange = (unsigned int)(254 - ref); break;
        case GR_FUNC_GEQUAL:  c.alphaLow = ref; c.alphaRange = (unsigned int)(255 - ref); break;
        default: goto jit;
        }
    }

    span(st, &c);
#if defined(GERBERA_CCSOFT_TOGGLE)
    ++g_grCcsoftStats[0];
#endif
    return 1;

jit:
#if defined(GERBERA_CCSOFT_TOGGLE)
    ++g_grCcsoftStats[1];
#endif
    return 0;
}

#endif /* GERBERA_CCSOFT */
