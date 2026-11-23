/* test_opengl.c - unit tests for src/gerbera/api_opengl (OpenGL ES 1.1 CM). */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "GLES/gl.h"
#include "gerbera/api_opengl/oglContext.h"

static int g_checks = 0;
static int g_fails  = 0;

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        ++g_checks;                                                     \
        if (!(cond))                                                    \
        {                                                               \
            ++g_fails;                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define NEAR_F(a, b, eps) (fabsf((float)(a) - (float)(b)) <= (eps))

static void init_dummy_matrix_stacks(oglContext_s *ctx, unsigned char *arena, int arenaSize)
{
    static const unsigned char depths[GR_MATRIXMODE_MAX] =
    {
        16, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    int i;

    grStackInit(&ctx->allocator, arena, arenaSize);
    for (i = 0; i < GR_MATRIXMODE_MAX; ++i)
    {
        int bytes = 4 + (int)depths[i] * (int)sizeof(_IMatrix4x4);
        oglMatrixStack_s *st = (oglMatrixStack_s *)grStackAllocate(&ctx->allocator, bytes);
        int j;
        st->index   = 0;
        st->depth   = depths[i];
        st->dirty   = 0;
        st->padding = 0;
        for (j = 0; j < (int)depths[i]; ++j)
            IMatrix4x4_identity(&st->elements[j]);
        ctx->matrixStack[i] = st;
    }
}

static void test_error_and_descriptor(void)
{
    oglContext_s ctx;
    _grSurfaceDescriptor desc;

    memset(&ctx, 0, sizeof(ctx));
    CHECK(ctx.error == OGL_ERROR_OK);

    /* Sticky first-error behavior per GLES 1.1 spec */
    oglSetError(&ctx, OGL_ERROR_INVALID_ENUM);
    CHECK(ctx.error == OGL_ERROR_INVALID_ENUM);
    oglSetError(&ctx, OGL_ERROR_OUT_OF_MEMORY);
    CHECK(ctx.error == OGL_ERROR_INVALID_ENUM);
    ctx.error = OGL_ERROR_OK;
    oglSetError(&ctx, OGL_ERROR_OUT_OF_MEMORY);
    CHECK(ctx.error == OGL_ERROR_OUT_OF_MEMORY);

    /* RGB565 */
    oglFillDescriptor(&desc, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
    CHECK(desc.colorBytes == 2);
    CHECK(desc.colorMask == 7);
    CHECK(desc.pixelFormat == 4);
    CHECK(desc.rgbaSizes[0] == 5 && desc.rgbaSizes[1] == 6 && desc.rgbaSizes[2] == 5 && desc.rgbaSizes[3] == 0);
    CHECK(desc.rgbaOffsets[0] == 11 && desc.rgbaOffsets[1] == 5 && desc.rgbaOffsets[2] == 0);

    /* RGBA4444 */
    oglFillDescriptor(&desc, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
    CHECK(desc.colorBytes == 2);
    CHECK(desc.colorMask == 15);
    CHECK(desc.pixelFormat == 5);
    CHECK(desc.rgbaSizes[0] == 4 && desc.rgbaSizes[1] == 4 && desc.rgbaSizes[2] == 4 && desc.rgbaSizes[3] == 4);
    CHECK(desc.rgbaOffsets[0] == 12 && desc.rgbaOffsets[1] == 8 && desc.rgbaOffsets[2] == 4 && desc.rgbaOffsets[3] == 0);

    /* RGBA5551 */
    oglFillDescriptor(&desc, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1);
    CHECK(desc.colorBytes == 2);
    CHECK(desc.colorMask == 15);
    CHECK(desc.pixelFormat == 5);
    CHECK(desc.rgbaSizes[0] == 5 && desc.rgbaSizes[1] == 5 && desc.rgbaSizes[2] == 5 && desc.rgbaSizes[3] == 1);
    CHECK(desc.rgbaOffsets[0] == 11 && desc.rgbaOffsets[1] == 6 && desc.rgbaOffsets[2] == 1 && desc.rgbaOffsets[3] == 0);
}

static void test_matrix_stack(void)
{
    static unsigned char arena[8192];
    oglContext_s ctx;
    oglMatrixStack_s *mvStack;
    _IMatrix4x4 *m;
    float outF[16];
    int outX[16];
    int fixedIn[16];
    float floatConverted[16];
    int i;

    memset(&ctx, 0, sizeof(ctx));
    init_dummy_matrix_stacks(&ctx, arena, (int)sizeof(arena));

    CHECK(oglIsMatrixStackValid(NULL) == 0);
    mvStack = oglGetMatrixStack(&ctx, GR_MATRIXMODE_MODELVIEW);
    CHECK(mvStack != NULL);
    CHECK(mvStack->depth == 16);
    CHECK(mvStack->dirty == 0);

    ctx.matrixMode = GR_MATRIXMODE_MODELVIEW;
    CHECK(oglGetActiveMatrixStack(&ctx) == mvStack);

    m = oglGetActiveMatrix(&ctx, 1);
    CHECK(m == &mvStack->elements[0]);
    CHECK(mvStack->dirty == 1);

    oglMatrixToOutput(outF, m, OGL_OUTPUTTYPE_FLOAT);
    oglMatrixToOutput(outX, m, OGL_OUTPUTTYPE_FIXED);
    for (i = 0; i < 16; ++i)
    {
        float want = (i % 5 == 0) ? 1.0f : 0.0f;
        CHECK(NEAR_F(outF[i], want, 1e-6f));
        CHECK(outX[i] == ((i % 5 == 0) ? 0x10000 : 0));
        fixedIn[i] = (i - 8) * 0x8000; /* -4.0 .. +3.5 in 16.16 */
    }

    oglMatrixFixedToFloat(floatConverted, fixedIn);
    CHECK(NEAR_F(floatConverted[0], -4.0f, 1e-5f));
    CHECK(NEAR_F(floatConverted[8], 0.0f, 1e-5f));
    CHECK(NEAR_F(floatConverted[9], 0.5f, 1e-5f));
    CHECK(NEAR_F(floatConverted[15], 3.5f, 1e-5f));
}

static void test_tex_manager(void)
{
    oglTextureManager *mgr;
    unsigned int names[24];
    int i;

    mgr = oglCreateTextureManager();
    CHECK(mgr != NULL);
    CHECK(oglIsValidTextureManager(mgr) == 1);
    /* Default texture 0 is allocated at creation */
    CHECK(mgr->numTextures == 1);
    CHECK(oglIsTexture(mgr, 0) == 1);
    CHECK(oglGetTexture(mgr, 0) != NULL);

    /* Generate & allocate 20 textures to trigger multiple power-of-2 hash growths */
    for (i = 0; i < 20; ++i)
    {
        names[i] = oglGenerateTextureName(mgr);
        CHECK(names[i] == (unsigned int)(i + 1));
        CHECK(oglAllocateTexture(mgr, names[i]) == 1);
        CHECK(oglIsTexture(mgr, names[i]) == 1);
    }
    CHECK(mgr->numTextures == 21);
    CHECK(mgr->capacity >= 32);

    /* Delete even-indexed textures, verify tombstones & remaining lookups */
    for (i = 0; i < 20; i += 2)
    {
        oglDeleteTexture(mgr, names[i]);
        CHECK(oglIsTexture(mgr, names[i]) == 0);
    }
    for (i = 1; i < 20; i += 2)
    {
        CHECK(oglIsTexture(mgr, names[i]) == 1);
    }
    CHECK(mgr->numTextures == 11);
    CHECK(mgr->numDeleted == 10);

    oglDestroyTextureManager(mgr);
}

static void test_bufobj_manager(void)
{
    oglContext_s ctx;
    oglBufObjManager_s *mgr;
    unsigned int b1, b2;
    oglBufObj_s *obj;
    int i;

    memset(&ctx, 0, sizeof(ctx));
    oglSetDefaultGetterValues(&ctx.getValues);

    mgr = oglCreateBufObjManager();
    CHECK(mgr != NULL);
    ctx.bufObjManager = mgr;
    CHECK(mgr->numObjects == 1);
    CHECK(oglIsBufObj(mgr, 0) == 1);

    b1 = oglGenerateBufObjName(mgr);
    CHECK(b1 == 1u);
    CHECK(oglAllocateBufObj(mgr, b1) == 1);

    b2 = oglGenerateBufObjName(mgr);
    CHECK(b2 == 2u);
    CHECK(oglAllocateBufObj(mgr, b2) == 1);

    /* Grow hash table across 16 additional VBOs */
    for (i = 0; i < 16; ++i)
    {
        unsigned int bn = oglGenerateBufObjName(mgr);
        CHECK(oglAllocateBufObj(mgr, bn) == 1);
    }
    CHECK(mgr->capacity >= 32);

    oglBindBuffer(&ctx, OGL_BUFOBJTARGET_ARRAY, b1);
    CHECK(ctx.activeBufObjs[OGL_BUFOBJTARGET_ARRAY] == b1);
    CHECK(ctx.getValues.arrayBufferBinding == (int)b1);

    oglBindBuffer(&ctx, OGL_BUFOBJTARGET_ELEMENT_ARRAY, b2);
    CHECK(ctx.activeBufObjs[OGL_BUFOBJTARGET_ELEMENT_ARRAY] == b2);
    CHECK(ctx.getValues.elementArrayBufferBinding == (int)b2);

    obj = oglGetActiveBufObj(&ctx, OGL_BUFOBJTARGET_ARRAY);
    CHECK(obj != NULL);
    CHECK(oglSetupBufObj(obj, OGL_BUFOBJUSAGE_STATIC_DRAW, 128) == 1);
    CHECK(obj->size == 128 && obj->buffer != NULL);
    /* Reallocate buffer data */
    CHECK(oglSetupBufObj(obj, OGL_BUFOBJUSAGE_DYNAMIC_DRAW, 64) == 1);
    CHECK(obj->size == 64 && obj->usage == OGL_BUFOBJUSAGE_DYNAMIC_DRAW);
    /* Zero-size buffer data frees storage */
    CHECK(oglSetupBufObj(obj, OGL_BUFOBJUSAGE_STATIC_DRAW, 0) == 1);
    CHECK(obj->size == 0 && obj->buffer == NULL);

    oglDeleteBufObj(mgr, b1);
    CHECK(oglIsBufObj(mgr, b1) == 0);

    oglDestroyBufObjManager(mgr);
}

static void test_getters(void)
{
    static unsigned char arena[8192];
    Gerbera_s gr;
    oglContext_s ctx;
    int outI[16];
    float outF[16];
    unsigned char outB[16];
    unsigned int uin[2];
    int svals[3];
    float unitIn[3];

    /* Pin exact 676-byte CodeView TI=0x149A layout */
    CHECK(sizeof(_oglGetterValues) == 0x2A4u);
    CHECK(offsetof(_oglGetterValues, colorWritemask) == 52u);
    CHECK(offsetof(_oglGetterValues, depthBits) == 56u);
    CHECK(offsetof(_oglGetterValues, stencilWritemask) == 336u);
    CHECK(offsetof(_oglGetterValues, pointSizeMin) == 344u);
    CHECK(offsetof(_oglGetterValues, textureCoordArraySize) == 396u);
    CHECK(offsetof(_oglGetterValues, aliasedPointSizeRange) == 444u);
    CHECK(offsetof(_oglGetterValues, viewport) == 508u);
    CHECK(offsetof(_oglGetterValues, currentColor) == 588u);
    CHECK(offsetof(_oglGetterValues, currentTextureCoords) == 604u);
    CHECK(offsetof(_oglGetterValues, compressedTextureFormats) == 636u);

    memset(&gr, 0, sizeof(gr));
    memset(&ctx, 0, sizeof(ctx));
    ctx.gerbera = &gr;
    init_dummy_matrix_stacks(&ctx, arena, (int)sizeof(arena));
    oglSetDefaultGetterValues(&ctx.getValues);

    /* Default GL state queries via oglGetInternal */
    oglGetInternal(&ctx, GL_MAX_LIGHTS, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 8);
    oglGetInternal(&ctx, GL_MAX_TEXTURE_SIZE, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 1024);
    oglGetInternal(&ctx, GL_DITHER, outB, OGL_OUTPUTTYPE_BOOL);
    CHECK(outB[0] == GL_TRUE);
    oglGetInternal(&ctx, GL_DEPTH_TEST, outB, OGL_OUTPUTTYPE_BOOL);
    CHECK(outB[0] == GL_FALSE);
    oglGetInternal(&ctx, GL_LIGHT_MODEL_AMBIENT, outF, OGL_OUTPUTTYPE_FLOAT);
    CHECK(NEAR_F(outF[0], 0.2f, 1e-6f) && NEAR_F(outF[3], 1.0f, 1e-6f));

    /* Unit-value converter: currentColor (1,1,1,1) -> INT 0x7FFFFFFF, FIXED 0x10000 */
    oglGetInternal(&ctx, GL_CURRENT_COLOR, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 0x7FFFFFFF && outI[3] == 0x7FFFFFFF);
    oglGetInternal(&ctx, GL_CURRENT_COLOR, outI, OGL_OUTPUTTYPE_FIXED);
    CHECK(outI[0] == 0x10000 && outI[3] == 0x10000);

    unitIn[0] = 0.0f;
    unitIn[1] = 0.5f;
    unitIn[2] = -1.0f;
    oglUnitValueToOutput(outI, unitIn, 3, OGL_OUTPUTTYPE_FIXED);
    CHECK(outI[0] == 0 && outI[1] == 0x8000 && outI[2] == -0x10000);
    oglUnitValueToOutput(outB, unitIn, 3, OGL_OUTPUTTYPE_BOOL);
    CHECK(outB[0] == 0 && outB[1] == 1 && outB[2] == 1);

    /* oglUIntToOutput handles unsigned values >= 0x80000000 */
    uin[0] = 42u;
    uin[1] = 0x80000000u;
    oglUIntToOutput(outF, uin, 2, OGL_OUTPUTTYPE_FLOAT);
    CHECK(NEAR_F(outF[0], 42.0f, 1e-3f));
    CHECK(NEAR_F(outF[1], 2147483648.0f, 1.0f));

    /* oglIScalarToOutput clamps overflow to INT32_MIN/MAX */
    svals[0] = IScalar_ftos(12.0f);
    svals[1] = IScalar_ftos(1.0e12f);
    svals[2] = IScalar_ftos(-1.0e12f);
    oglIScalarToOutput(outI, svals, 3, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 12);
    CHECK(outI[1] == 0x7FFFFFFF);
    CHECK(outI[2] == (int)0x80000000u);

    /* Per-activeTexture & per-clientActiveTexture indexed state */
    {
        int b0 = 7, b1 = 19;
        float tc1[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
        int sz0 = 2, sz1 = 3;

        oglSetGetterValueInt(&ctx, 0, GL_TEXTURE_BINDING_2D, &b0);
        oglSetGetterValueInt(&ctx, 1, GL_TEXTURE_BINDING_2D, &b1);
        oglSetGetterValueFloat(&ctx, 1, GL_CURRENT_TEXTURE_COORDS, tc1);
        oglSetGetterValueInt(&ctx, 0, GL_TEXTURE_COORD_ARRAY_SIZE, &sz0);
        oglSetGetterValueInt(&ctx, 1, GL_TEXTURE_COORD_ARRAY_SIZE, &sz1);

        ctx.activeTexture = 0;
        oglGetInternal(&ctx, GL_TEXTURE_BINDING_2D, outI, OGL_OUTPUTTYPE_INT);
        CHECK(outI[0] == 7);
        ctx.activeTexture = 1;
        oglGetInternal(&ctx, GL_TEXTURE_BINDING_2D, outI, OGL_OUTPUTTYPE_INT);
        CHECK(outI[0] == 19);
        oglGetInternal(&ctx, GL_CURRENT_TEXTURE_COORDS, outF, OGL_OUTPUTTYPE_FLOAT);
        CHECK(NEAR_F(outF[0], 0.25f, 1e-6f) && NEAR_F(outF[2], 0.75f, 1e-6f));

        ctx.clientActiveTexture = 0;
        oglGetInternal(&ctx, GL_TEXTURE_COORD_ARRAY_SIZE, outI, OGL_OUTPUTTYPE_INT);
        CHECK(outI[0] == 2);
        ctx.clientActiveTexture = 1;
        oglGetInternal(&ctx, GL_TEXTURE_COORD_ARRAY_SIZE, outI, OGL_OUTPUTTYPE_INT);
        CHECK(outI[0] == 3);
    }

    /* Matrix & stack depth queries */
    ctx.matrixMode[&ctx.matrixStack[0]->index] = 0;
    ctx.matrixStack[GR_MATRIXMODE_MODELVIEW]->index = 3;
    oglGetInternal(&ctx, GL_MODELVIEW_STACK_DEPTH, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 4);
    oglGetInternal(&ctx, GL_MODELVIEW_MATRIX, outF, OGL_OUTPUTTYPE_FLOAT);
    CHECK(NEAR_F(outF[0], 1.0f, 1e-6f) && NEAR_F(outF[15], 1.0f, 1e-6f));

    /* Stencil ref / clear value masking against drawSurface.stencilBytes */
    gr.stencilReference = 0x1AB;
    gr.clearStencil     = 0x1CD;
    gr.drawSurface.stencilBytes = 0;
    oglGetInternal(&ctx, GL_STENCIL_REF, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 0);
    gr.drawSurface.stencilBytes = 1;
    oglGetInternal(&ctx, GL_STENCIL_REF, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 0xAB);
    oglGetInternal(&ctx, GL_STENCIL_CLEAR_VALUE, outI, OGL_OUTPUTTYPE_INT);
    CHECK(outI[0] == 0xCD);

    /* Error recording on NULL params & invalid enum */
    ctx.error = OGL_ERROR_OK;
    oglGetInternal(&ctx, GL_MAX_LIGHTS, NULL, OGL_OUTPUTTYPE_INT);
    CHECK(ctx.error == OGL_ERROR_INVALID_VALUE);

    ctx.error = OGL_ERROR_OK;
    oglGetInternal(&ctx, 0xDEADu, outI, OGL_OUTPUTTYPE_INT);
    CHECK(ctx.error == OGL_ERROR_INVALID_ENUM);
}

int main(void)
{
    test_error_and_descriptor();
    test_matrix_stack();
    test_tex_manager();
    test_bufobj_manager();
    test_getters();

    printf("opengl tests: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
