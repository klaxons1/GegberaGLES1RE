/* eglUtil.c - Gerbera EGL 1.1 config, refcount, surface descriptor, and blit
 * helpers (ported from eglUtil.inl & PE .rdata 0x470128..0x47021F).
 */
#include "gerbera/egl/eglInternal.h"
#include "hg/hgInt32.h"
#include "hg/hgMemory.h"

/* --- PE .rdata config tables (0x470128..0x47021F) -------------------------- */
const ColorFormat g_colorFormats[EGL_NUM_COLOR_FORMATS] =
{
    { 2, 4, 4, 4, 4, 0, 0, 0 }, /* RGBA4444 (16-bit) */
    { 2, 5, 6, 5, 0, 0, 0, 0 }, /* RGB565   (16-bit) */
    { 4, 8, 8, 8, 0, 1, 0, 0 }, /* RGB888   (32-bit, bindToRgb=1) */
    { 4, 8, 8, 8, 8, 1, 1, 0 }  /* RGBA8888 (32-bit, bindToRgb=1, bindToRgba=1) */
};

const char g_depthFormats[EGL_NUM_DEPTH_FORMATS]     = { 0, 2, 4 };
const char g_stencilFormats[EGL_NUM_STENCIL_FORMATS] = { 0, 1 };
const char g_surfaceTypes[EGL_NUM_SURFACE_TYPES]     = { EGL_PBUFFER_BIT, EGL_PIXMAP_BIT, EGL_WINDOW_BIT };

/* 0x470160 */
const eglConfig s_defaultConfig =
{
    0,              /* colorBits */
    0,              /* alphaBits */
    0,              /* blueBits */
    0,              /* greenBits */
    0,              /* redBits */
    EGL_NONE,       /* configCaveat */
    0,              /* configID */
    0,              /* depthBits */
    0,              /* level */
    EGL_FALSE,      /* nativeRenderable */
    EGL_NONE,       /* nativeVisualType */
    0,              /* nativeVisualID */
    1,              /* numSampleBuffers */
    1,              /* numSamples */
    0,              /* stencilBits */
    0,              /* surfaceType */
    EGL_NONE,       /* transparentType */
    0,              /* transparentRedValue */
    0,              /* transparentGreenValue */
    0,              /* transparentBlueValue */
    EGL_FALSE,      /* bindToTextureRgb */
    EGL_FALSE,      /* bindToTextureRgba */
    1,              /* minSwapInterval */
    1               /* maxSwapInterval */
};

/* 0x4701C0 */
const eglConfig s_defaultFilter =
{
    0,              /* colorBits */
    0,              /* alphaBits */
    0,              /* blueBits */
    0,              /* greenBits */
    0,              /* redBits */
    EGL_DONT_CARE,  /* configCaveat */
    EGL_DONT_CARE,  /* configID */
    0,              /* depthBits */
    0,              /* level */
    EGL_DONT_CARE,  /* nativeRenderable */
    EGL_DONT_CARE,  /* nativeVisualType */
    EGL_DONT_CARE,  /* nativeVisualID */
    0,              /* numSampleBuffers */
    0,              /* numSamples */
    0,              /* stencilBits */
    EGL_WINDOW_BIT, /* surfaceType */
    EGL_NONE,       /* transparentType */
    EGL_DONT_CARE,  /* transparentRedValue */
    EGL_DONT_CARE,  /* transparentGreenValue */
    EGL_DONT_CARE,  /* transparentBlueValue */
    EGL_DONT_CARE,  /* bindToTextureRgb */
    EGL_DONT_CARE,  /* bindToTextureRgba */
    EGL_DONT_CARE,  /* minSwapInterval */
    EGL_DONT_CARE   /* maxSwapInterval */
};

static int unequal(int configVal, int filterVal)
{
    if (filterVal == EGL_DONT_CARE)
        return 0;
    return configVal != filterVal;
}

static int smaller(int configVal, int filterVal)
{
    if (filterVal == EGL_DONT_CARE)
        return 0;
    return configVal < filterVal;
}

void eglInitRefCount(eglRefCount *refCount)
{
    refCount->referenceCount = 0;
    refCount->destroyed = 0;
}

void eglAddRef(eglRefCount *refCount)
{
    ++refCount->referenceCount;
    EGL_ASSERT_AT("eglUtil.inl", 107, "refCount->referenceCount > 0",
                  refCount->referenceCount > 0);
}

int eglDestroyRef(eglRefCount *refCount)
{
    EGL_ASSERT_AT("eglUtil.inl", 113, "refCount->referenceCount >= 0",
                  refCount->referenceCount >= 0);
    refCount->destroyed = 1;
    return refCount->referenceCount == 0;
}

int eglReleaseRef(eglRefCount *refCount)
{
    EGL_ASSERT_AT("eglUtil.inl", 120, "refCount->referenceCount > 0",
                  refCount->referenceCount > 0);
    --refCount->referenceCount;
    return refCount->referenceCount == 0 && refCount->destroyed != 0;
}

void eglDisplayAddSurface(eglDisplay_s *display, eglSurface_s *surface)
{
    surface->display = display;
    surface->id = ++display->surfaceCount;
    surface->prev = NULL;
    surface->next = display->surfaceList;
    if (display->surfaceList != NULL)
        display->surfaceList->prev = surface;
    display->surfaceList = surface;
}

void eglDisplayRemoveSurface(eglDisplay_s *display, eglSurface_s *surface)
{
    if (display->surfaceList == surface)
        display->surfaceList = surface->next;
    else
        surface->prev->next = surface->next;

    if (surface->next != NULL)
        surface->next->prev = surface->prev;

    surface->prev = (eglSurface_s *)EGL_POISON_PTR;
    surface->next = (eglSurface_s *)EGL_POISON_PTR;
}

void eglDisplayRemoveContext(eglDisplay_s *display, eglContext_s *context)
{
    if (display->contextList == context)
        display->contextList = context->next;
    else
        context->prev->next = context->next;

    if (context->next != NULL)
        context->next->prev = context->prev;

    context->prev = (eglContext_s *)EGL_POISON_PTR;
    context->next = (eglContext_s *)EGL_POISON_PTR;
}

void eglFreeSurface(eglSurface_s *surface)
{
    EGL_ASSERT_AT("eglUtil.inl", 180, "surface", surface != NULL);
    EGL_ASSERT_AT("eglUtil.inl", 181,
                  "surface->refCount.referenceCount == 0 && surface->refCount.destroyed",
                  surface->refCount.referenceCount == 0 && surface->refCount.destroyed != 0);

    switch (surface->type)
    {
    case EGL_SURFACE_WINDOW:
        egliWindowSurfaceDestroy((eglWindowSurface *)surface);
        break;
    case EGL_SURFACE_PBUFFER:
    {
        eglPbufferSurface *pbuffer = (eglPbufferSurface *)surface;
        /* The original releases by texture name, even if that name now holds
         * another pbuffer's image. Preserve this likely lifetime bookkeeping bug. */
        if (pbuffer->boundESContext != NULL)
            oglReleaseSurfaceTexture(pbuffer->boundESContext, pbuffer->textureName,
                                     pbuffer->mipmapLevel);
        pbuffer->boundESContext = NULL;
        if (surface->colorBuffer != NULL)
        {
            eglFree(surface->colorBuffer);
            surface->colorBuffer = (void *)EGL_POISON_PTR;
        }
        break;
    }
    case EGL_SURFACE_PIXMAP:
        egliPixmapSurfaceDestroy((eglPixmapSurface *)surface);
        break;
    default:
        EGL_ASSERT_AT("eglUtil.inl", 204, "0", 0);
        break;
    }

    if (surface->depthBuffer != NULL)
    {
        eglFree(surface->depthBuffer);
        surface->depthBuffer = (void *)EGL_POISON_PTR;
    }
    if (surface->stencilBuffer != NULL)
    {
        eglFree(surface->stencilBuffer);
        surface->stencilBuffer = (void *)EGL_POISON_PTR;
    }

    eglDisplayRemoveSurface(surface->display, surface);
    surface->display = (eglDisplay_s *)EGL_POISON_PTR;
    eglFree(surface);
}

void eglGetConfigIndices(ConfigIndices *indices, int configID)
{
    enum {
        CONFIGS_PER_DEPTH = EGL_NUM_STENCIL_FORMATS * EGL_NUM_SURFACE_TYPES,
        CONFIGS_PER_COLOR = EGL_NUM_DEPTH_FORMATS * CONFIGS_PER_DEPTH
    };
    int idx;

    EGL_ASSERT_AT("eglUtil.inl", 232, "indices", indices != NULL);
    EGL_ASSERT_AT("eglUtil.inl", 233, "hgInRange32(configID, 1, EGL_NUM_BUFFER_FORMATS)",
                  hgInRange32(configID, 1, EGL_NUM_BUFFER_FORMATS) != 0);

    idx = configID - 1;
    indices->colorIndex = idx / CONFIGS_PER_COLOR;
    idx %= CONFIGS_PER_COLOR;
    indices->depthIndex = idx / CONFIGS_PER_DEPTH;
    idx %= CONFIGS_PER_DEPTH;
    indices->stencilIndex = idx / EGL_NUM_SURFACE_TYPES;
    indices->surfaceTypeIndex = idx % EGL_NUM_SURFACE_TYPES;

    EGL_ASSERT_AT("eglUtil.inl", 248, "hgInRange32(indices->colorIndex, 0, EGL_NUM_COLOR_FORMATS - 1)",
                  hgInRange32(indices->colorIndex, 0, EGL_NUM_COLOR_FORMATS - 1) != 0);
    EGL_ASSERT_AT("eglUtil.inl", 249, "hgInRange32(indices->depthIndex, 0, EGL_NUM_DEPTH_FORMATS - 1)",
                  hgInRange32(indices->depthIndex, 0, EGL_NUM_DEPTH_FORMATS - 1) != 0);
    EGL_ASSERT_AT("eglUtil.inl", 250, "hgInRange32(indices->stencilIndex, 0, EGL_NUM_STENCIL_FORMATS - 1)",
                  hgInRange32(indices->stencilIndex, 0, EGL_NUM_STENCIL_FORMATS - 1) != 0);
    EGL_ASSERT_AT("eglUtil.inl", 251, "hgInRange32(indices->surfaceTypeIndex, 0, EGL_NUM_SURFACE_TYPES - 1)",
                  hgInRange32(indices->surfaceTypeIndex, 0, EGL_NUM_SURFACE_TYPES - 1) != 0);
}

void eglFillConfig(eglConfig *config, int configID)
{
    ConfigIndices indices;
    const ColorFormat *cf;

    EGL_ASSERT_AT("eglUtil.inl", 258, "config", config != NULL);
    EGL_ASSERT_AT("eglUtil.inl", 259, "hgInRange32(configID, 1, EGL_NUM_BUFFER_FORMATS)",
                  hgInRange32(configID, 1, EGL_NUM_BUFFER_FORMATS) != 0);

    *config = s_defaultConfig;
    config->configID = configID;

    eglGetConfigIndices(&indices, configID);
    cf = &g_colorFormats[indices.colorIndex];
    config->redBits = cf->redBits;
    config->greenBits = cf->greenBits;
    config->blueBits = cf->blueBits;
    config->alphaBits = cf->alphaBits;
    config->colorBits = cf->alphaBits + cf->blueBits + cf->greenBits + cf->redBits;
    config->depthBits = 8 * g_depthFormats[indices.depthIndex];
    config->stencilBits = 8 * g_stencilFormats[indices.stencilIndex];
    config->surfaceType = g_surfaceTypes[indices.surfaceTypeIndex];

    config->bindToTextureRgb = cf->bindToRgb;
    config->bindToTextureRgba = cf->bindToRgba;

    egliFixConfig(config);
}

int eglCompareConfigs(const eglConfig *config1, const eglConfig *config2, const eglConfig *filter)
{
    unsigned int sum1 = 0, sum2 = 0;
    int caveat1 = config1->configCaveat, caveat2 = config2->configCaveat;
    if ((caveat1 == EGL_NONE && (caveat2 == EGL_SLOW_CONFIG || caveat2 == EGL_NON_CONFORMANT_CONFIG)) ||
        (caveat1 == EGL_SLOW_CONFIG && caveat2 == EGL_NON_CONFORMANT_CONFIG))
        return -1;
    if ((caveat2 == EGL_NONE && (caveat1 == EGL_SLOW_CONFIG || caveat1 == EGL_NON_CONFORMANT_CONFIG)) ||
        (caveat2 == EGL_SLOW_CONFIG && caveat1 == EGL_NON_CONFORMANT_CONFIG))
        return 1;

#define ADD_CHANNEL(field) \
    if (filter->field != 0 && filter->field != EGL_DONT_CARE) \
    { sum1 += (unsigned int)config1->field; sum2 += (unsigned int)config2->field; }
    if (filter != NULL)
    {
        ADD_CHANNEL(redBits);
        ADD_CHANNEL(greenBits);
        ADD_CHANNEL(blueBits);
        ADD_CHANNEL(alphaBits);
    }
#undef ADD_CHANNEL
    if ((int)sum1 != (int)sum2)
        return (int)sum1 > (int)sum2 ? -1 : 1;

#define COMPARE_FIELD(field) \
    if (config1->field != config2->field) return config1->field < config2->field ? -1 : 1
    COMPARE_FIELD(colorBits);
    COMPARE_FIELD(numSampleBuffers);
    COMPARE_FIELD(numSamples);
    COMPARE_FIELD(depthBits);
    COMPARE_FIELD(stencilBits);
    COMPARE_FIELD(configID);
#undef COMPARE_FIELD
    return 0;
}

int eglFilterConfig(const eglConfig *config, const eglConfig *filter)
{
    if (!eglIsValidConfigID(config->configID))
        return 0;
    if (filter->configID == config->configID)
        return 1;

    if (smaller(config->colorBits, filter->colorBits) ||
        smaller(config->redBits, filter->redBits) ||
        smaller(config->greenBits, filter->greenBits) ||
        smaller(config->blueBits, filter->blueBits) ||
        smaller(config->alphaBits, filter->alphaBits) ||
        unequal(config->configCaveat, filter->configCaveat) ||
        unequal(config->configID, filter->configID) ||
        smaller(config->depthBits, filter->depthBits) ||
        config->level != filter->level ||
        unequal(config->nativeRenderable, filter->nativeRenderable) ||
        unequal(config->nativeVisualType, filter->nativeVisualType) ||
        unequal(config->nativeVisualID, filter->nativeVisualID) ||
        smaller(config->numSampleBuffers, filter->numSampleBuffers) ||
        smaller(config->numSamples, filter->numSamples) ||
        smaller(config->stencilBits, filter->stencilBits) ||
        (filter->surfaceType != EGL_DONT_CARE && (config->surfaceType & filter->surfaceType) != filter->surfaceType) ||
        unequal(config->transparentType, filter->transparentType) ||
        unequal(config->bindToTextureRgb, filter->bindToTextureRgb) ||
        unequal(config->bindToTextureRgba, filter->bindToTextureRgba) ||
        unequal(config->minSwapInterval, filter->minSwapInterval) ||
        unequal(config->maxSwapInterval, filter->maxSwapInterval))
    {
        return 0;
    }

    /* The original ignores transparent RGB component values. */
    return 1;
}

int eglUpdateSurfaceBuffers(eglSurface_s *surface)
{
    ConfigIndices indices;
    int width = 0;
    int height = 0;
    int depthBytes;
    int stencilBytes;
    int valid = 1;
    unsigned int depthSize;
    unsigned int stencilSize;
    void *depthBuffer;
    void *stencilBuffer;

    EGL_ASSERT_AT("eglUtil.inl", 408, "surface", surface != NULL);

    switch (surface->type)
    {
    case EGL_SURFACE_WINDOW:
        valid = egliIsValidNativeWindow(((eglWindowSurface *)surface)->nativeWindow);
        if (valid)
            egliGetNativeWindowSize((eglWindowSurface *)surface, &width, &height);
        break;
    case EGL_SURFACE_PBUFFER:
        width = surface->width;
        height = surface->height;
        break;
    case EGL_SURFACE_PIXMAP:
        valid = egliIsValidNativePixmap(((eglPixmapSurface *)surface)->nativePixmap);
        if (valid)
            egliGetNativePixmapSize(((eglPixmapSurface *)surface)->nativePixmap, &width, &height);
        break;
    default:
        EGL_ASSERT_AT("eglUtil.inl", 425, "0", 0);
        break;
    }

    if (width != surface->width || height != surface->height)
    {
        if (surface->depthBuffer != NULL)
        {
            eglFree(surface->depthBuffer);
            surface->depthBuffer = NULL;
        }
        if (surface->stencilBuffer != NULL)
        {
            eglFree(surface->stencilBuffer);
            surface->stencilBuffer = NULL;
        }
        surface->width = width;
        surface->height = height;
        if (egliSurfaceResized(surface) == 0)
            return 0;
    }

    if (!valid)
        return 0;

    eglGetConfigIndices(&indices, surface->configID);
    depthBytes = g_depthFormats[indices.depthIndex];
    stencilBytes = g_stencilFormats[indices.stencilIndex];

    depthSize = (unsigned int)height * (unsigned int)width * (unsigned int)depthBytes;
    stencilSize = (unsigned int)height * (unsigned int)width * (unsigned int)stencilBytes;
    depthBuffer = surface->depthBuffer;
    stencilBuffer = surface->stencilBuffer;
    if (depthBuffer == NULL && depthSize != 0)
        depthBuffer = eglAlloc(depthSize);
    if (stencilBuffer == NULL && stencilSize != 0)
        stencilBuffer = eglAlloc(stencilSize);
    if ((depthBuffer == NULL && depthSize != 0) ||
        (stencilBuffer == NULL && stencilSize != 0))
    {
        eglFree(depthBuffer);
        eglFree(stencilBuffer);
        surface->depthBuffer = NULL;
        surface->stencilBuffer = NULL;
        return 0;
    }
    surface->depthBuffer = depthBuffer;
    surface->stencilBuffer = stencilBuffer;
    return 1;
}

void eglSetSurfaceDescriptor(oglSurfaceDescriptor *desc, const eglSurface_s *surface, const eglConfig *config)
{
    EGL_ASSERT_AT("eglUtil.inl", 483, "desc && surface && config",
                  desc != NULL && surface != NULL && config != NULL);

    hgMemSet(desc, 0, sizeof(*desc));
    desc->redBits = config->redBits;
    desc->greenBits = config->greenBits;
    desc->blueBits = config->blueBits;
    desc->alphaBits = config->alphaBits;
    desc->redOffset = config->blueBits + config->greenBits;
    desc->greenOffset = config->blueBits;
    desc->blueOffset = 0;
    desc->alphaOffset = config->blueBits + config->greenBits + config->redBits;
    egliFixPixelOffsets(desc, surface);

    desc->colorBytes = egliNextPowerOfTwo((config->colorBits - 1) >> 3);
    desc->depthBytes = config->depthBits >> 3;
    desc->stencilBytes = config->stencilBits >> 3;
    desc->width = surface->width;
    desc->height = surface->height;
    desc->colorBuffer = surface->colorBuffer;
    desc->colorBufferStride = surface->colorBufferStride;
    desc->depthBuffer = surface->depthBuffer;
    desc->depthBufferStride = surface->width * (config->depthBits >> 3);
    desc->stencilBuffer = surface->stencilBuffer;
    desc->stencilBufferStride = surface->width * (config->stencilBits >> 3);
}

int eglUpdateSurfaces(oglContext_s *esContext, eglSurface_s *drawSurface, eglSurface_s *readSurface)
{
    oglSurfaceDescriptor drawDesc;
    oglSurfaceDescriptor readDesc;
    eglConfig config;

    EGL_ASSERT_AT("eglUtil.inl", 1076, "drawSurface && readSurface",
                  drawSurface != NULL && readSurface != NULL);
    EGL_ASSERT_AT("eglUtil.inl", 1077, "drawSurface->configID == readSurface->configID",
                  drawSurface->configID == readSurface->configID);

    hgMemSet(&drawDesc, 0, sizeof(drawDesc));
    hgMemSet(&readDesc, 0, sizeof(readDesc));
    eglFillConfig(&config, drawSurface->configID);

    if (eglUpdateSurfaceBuffers(drawSurface) != 0 &&
        eglUpdateSurfaceBuffers(readSurface) != 0)
    {
        eglSetSurfaceDescriptor(&drawDesc, drawSurface, &config);
        eglSetSurfaceDescriptor(&readDesc, readSurface, &config);
        oglSetBuffers(esContext, &drawDesc, &readDesc);
        return 1;
    }

    oglSetBuffers(esContext, &drawDesc, &readDesc);
    return 0;
}

void eglFreeContext(eglContext_s *context)
{
    eglSurface_s *surface;

    EGL_ASSERT_AT("eglUtil.inl", 545, "context", context != NULL);
    EGL_ASSERT_AT("eglUtil.inl", 546,
                  "context->refCount.referenceCount == 0 && context->refCount.destroyed",
                  context->refCount.referenceCount == 0 && context->refCount.destroyed != 0);

    /* Keep pbuffer references from outliving their context, including deferred
     * destruction. Clearing another context's binding would lose the reference
     * needed to detach its texture before the surface storage is freed. */
    for (surface = context->display->surfaceList; surface != NULL; surface = surface->next)
    {
        if (surface->type == EGL_SURFACE_PBUFFER)
        {
            eglPbufferSurface *pbuffer = (eglPbufferSurface *)surface;
            if (pbuffer->boundESContext == context->esContext)
            {
                if (!oglIsSurfaceTextureReleased(pbuffer->boundESContext, pbuffer->textureName))
                    oglReleaseSurfaceTexture(pbuffer->boundESContext, pbuffer->textureName,
                                             pbuffer->mipmapLevel);
                pbuffer->boundESContext = NULL;
                pbuffer->textureName = 0;
            }
        }
    }

    oglDestroyContext(context->esContext);
    context->esContext = (oglContext_s *)EGL_POISON_PTR;
    eglDisplayRemoveContext(context->display, context);
    context->display = (eglDisplay_s *)EGL_POISON_PTR;
    eglFree(context);
}

void eglBlit(void *dstBuffer, int dstStride, const void *srcBuffer, int srcStride,
             int width, int height, int srcBytesPerPixel, int dstBytesPerPixel)
{
    const unsigned char *srcRow = (const unsigned char *)srcBuffer;
    unsigned char *dstRow = (unsigned char *)dstBuffer;
    int x, y;

    EGL_ASSERT_AT("eglUtil.inl", 564, "dstBuffer && srcBuffer",
                  dstBuffer != NULL && srcBuffer != NULL);
    EGL_ASSERT_AT("eglUtil.inl", 565, "srcBytesPerPixel == dstBytesPerPixel",
                  srcBytesPerPixel == dstBytesPerPixel);

    if (srcBytesPerPixel == dstBytesPerPixel)
    {
        int rowBytes = width * srcBytesPerPixel;
        if (srcStride == dstStride && srcStride == rowBytes)
        {
            if (rowBytes * height > 0)
                hgMemMove(dstBuffer, srcBuffer, rowBytes * height);
            return;
        }
        if (rowBytes > 0)
        {
            for (y = 0; y < height; ++y)
            {
                hgMemMove(dstRow, srcRow, rowBytes);
                srcRow += srcStride;
                dstRow += dstStride;
            }
        }
        return;
    }

    switch (srcBytesPerPixel)
    {
    case 1:
        for (y = 0; y < height; ++y)
        {
            for (x = 0; x < width; ++x)
                dstRow[x] = srcRow[x];
            srcRow += srcStride;
            dstRow += dstStride;
        }
        break;
    case 2:
        for (y = 0; y < height; ++y)
        {
            const unsigned short *src16 = (const unsigned short *)srcRow;
            unsigned short *dst16 = (unsigned short *)dstRow;
            for (x = 0; x < width; ++x)
                dst16[x] = src16[x];
            srcRow += srcStride;
            dstRow += dstStride;
        }
        break;
    case 4:
        for (y = 0; y < height; ++y)
        {
            const unsigned int *src32 = (const unsigned int *)srcRow;
            unsigned int *dst32 = (unsigned int *)dstRow;
            for (x = 0; x < width; ++x)
                dst32[x] = src32[x];
            srcRow += srcStride;
            dstRow += dstStride;
        }
        break;
    default:
        EGL_ASSERT_AT("eglUtil.inl", 614, "0", 0);
        break;
    }
}
