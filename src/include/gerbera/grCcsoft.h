/* grCcsoft.h - CCsoft-style specialised C pixel spans (GERBERA_CCSOFT builds).
 *
 * The JIT pixel pipeline keeps every interpolant in memory and expands and
 * repacks RGB565 per pixel. For the common fixed-function states (RGB565
 * colour buffer, nearest-sampled repeat textures, MODULATE/REPLACE, Z16,
 * alpha test, blending, linear-gradient fog, ordered dither) grCcsoft.c
 * renders the pixel pipe's scanlines with C spans compiled per state class,
 * the way CCsoftgl's rasterizer does. They reproduce the JIT's arithmetic
 * bit for bit (verified against the ARM JIT by symbian/emu); any other
 * state falls back to the JIT.
 */
#ifndef GERBERA_GRCCSOFT_H
#define GERBERA_GRCCSOFT_H

#if defined(GERBERA_CCSOFT)

struct Gerbera_s;

/* Per-context span selection, rebuilt by grCcsoftClassify whenever the pixel
 * pipeline is revalidated (Gerbera_s.ccsoft). Values that can change without
 * a revalidation (alpha reference, fog range, current colour) are read per
 * primitive instead. */
typedef struct grCcsoftState_s
{
    unsigned int key;          /* state class; 0 = the JIT renders */
    void *span[3];             /* white, flat and smooth colour spans */
    int gradientMask;          /* activeGradientMask at classification */
    int tex;                   /* key texture field */
    int needAlpha;             /* alpha blending or alpha test */
    int colorUse;              /* gradientMask & the colour gradients the class reads */
    int smoothOk;              /* every colour gradient the class reads is set up */
    int depthLess, depthEqual, depthGreater, depthWrite;
    int srcA, srcB, dstA, dstB;   /* blend weight + 1 = A + B * source alpha */
    int fogR, fogG, fogB;
    int replace;               /* texture environment REPLACE */
    int textureFormat;
} grCcsoftState;

/* Called by grEnsureValidPixelPipeline once the pipeline is valid: decides
 * whether the current state can use a C span (state->ccsoft). */
void grCcsoftClassify(struct Gerbera_s *state);

/* Renders the pixel pipe's scanlines; returns 0 when the JIT must do it. */
int grCcsoftExecute(struct Gerbera_s *state);

/* Builds with GERBERA_CCSOFT_TOGGLE (emulator/host tests) can switch the
 * spans off at run time to compare against the JIT. The Symbian DLL has no
 * writable static data, so there the spans are always on. */
#if defined(GERBERA_CCSOFT_TOGGLE)
extern int g_grCcsoftEnabled;
extern unsigned int g_grCcsoftStats[2]; /* calls taken by C spans / by the JIT */
#define GR_CCSOFT_ENABLED (g_grCcsoftEnabled != 0)
#else
#define GR_CCSOFT_ENABLED 1
#endif

#endif /* GERBERA_CCSOFT */
#endif /* GERBERA_GRCCSOFT_H */
