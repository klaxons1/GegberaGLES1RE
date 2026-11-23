/* eglApi.c - Gerbera EGL 1.1 pbuffer, context, and current-context entry
 * points (ported from eglApi.inl).
 */
#include "gerbera/egl/eglInternal.h"
#include "hg/hgInt32.h"
#include "hg/hgMemory.h"

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    eglDisplay_s *display;
    eglConfig cfg;
    eglPbufferSurface *pbuffer;
    int width = 0;
    int height = 0;
    int largestPbuffer = 0;
    int textureFormat = EGL_NO_TEXTURE;
    int textureTarget = EGL_NO_TEXTURE;
    int mipmapTexture = 0;
    int bytesPerPixel;
    void *colorBuffer = NULL;

    if (egliGetState() == NULL)
    {
        eglSetError(EGL_NOT_INITIALIZED);
        return EGL_NO_SURFACE;
    }

    eglSetError(EGL_SUCCESS);
    display = eglMapDisplay(dpy);
    if (display == NULL)
    {
        eglSetError(EGL_BAD_DISPLAY);
        return EGL_NO_SURFACE;
    }

    if (eglIsValidConfigID(config) == 0)
    {
        eglSetError(EGL_BAD_CONFIG);
        return EGL_NO_SURFACE;
    }

    eglFillConfig(&cfg, config);
    if ((cfg.surfaceType & EGL_PBUFFER_BIT) == 0)
    {
        eglSetError(EGL_BAD_MATCH);
        return EGL_NO_SURFACE;
    }

    if (attrib_list != NULL)
    {
        while (*attrib_list != EGL_NONE)
        {
            EGLint attr = attrib_list[0];
            EGLint val  = attrib_list[1];

            switch (attr)
            {
            case EGL_HEIGHT:
                height = val;
                if (val < 0)
                {
                    eglSetError(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_SURFACE;
                }
                break;
            case EGL_WIDTH:
                width = val;
                if (val < 0)
                {
                    eglSetError(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_SURFACE;
                }
                break;
            case EGL_LARGEST_PBUFFER:
                largestPbuffer = (val != 0);
                break;
            case EGL_TEXTURE_FORMAT:
                textureFormat = val;
                if (val != EGL_NO_TEXTURE && val != EGL_TEXTURE_RGB && val != EGL_TEXTURE_RGBA)
                {
                    eglSetError(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_SURFACE;
                }
                break;
            case EGL_TEXTURE_TARGET:
                textureTarget = val;
                if (val != EGL_NO_TEXTURE && val != EGL_TEXTURE_2D)
                {
                    eglSetError(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_SURFACE;
                }
                break;
            case EGL_MIPMAP_TEXTURE:
                mipmapTexture = (val != 0);
                break;
            default:
                eglSetError(EGL_BAD_ATTRIBUTE);
                return EGL_NO_SURFACE;
            }
            attrib_list += 2;
        }
    }

    if ((cfg.bindToTextureRgb == 0 && textureFormat == EGL_TEXTURE_RGB) ||
        (cfg.bindToTextureRgba == 0 && textureFormat == EGL_TEXTURE_RGBA) ||
        ((textureFormat == EGL_NO_TEXTURE) != (textureTarget == EGL_NO_TEXTURE)))
    {
        eglSetError(EGL_BAD_MATCH);
        return EGL_NO_SURFACE;
    }

    if (textureTarget == EGL_TEXTURE_2D &&
        (hgIsPowerOfTwo32((unsigned int)width) == 0 ||
         hgIsPowerOfTwo32((unsigned int)height) == 0))
    {
        eglSetError(EGL_BAD_MATCH);
        return EGL_NO_SURFACE;
    }

    if (width > EGL_MAX_PBUFFER_DIM || height > EGL_MAX_PBUFFER_DIM)
    {
        if (largestPbuffer == 0)
        {
            eglSetError(EGL_BAD_ALLOC);
            return EGL_NO_SURFACE;
        }
        if (width > EGL_MAX_PBUFFER_DIM)
            width = EGL_MAX_PBUFFER_DIM;
        if (height > EGL_MAX_PBUFFER_DIM)
            height = EGL_MAX_PBUFFER_DIM;
    }

    pbuffer = (eglPbufferSurface *)eglAlloc(sizeof(eglPbufferSurface));
    if (pbuffer == NULL)
    {
        eglSetError(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }

    bytesPerPixel = egliNextPowerOfTwo((cfg.colorBits - 1) >> 3);
    if (width != 0 && height != 0)
    {
        for (;;)
        {
            colorBuffer = eglAlloc((unsigned int)(bytesPerPixel * width * height));
            if (colorBuffer != NULL || largestPbuffer == 0 || width <= 1 || height <= 1)
                break;
            if (width >= height)
                width >>= 1;
            else
                height >>= 1;
        }
        if (colorBuffer == NULL)
        {
            eglFree(pbuffer);
            eglSetError(EGL_BAD_ALLOC);
            return EGL_NO_SURFACE;
        }
    }

    hgMemSet(pbuffer, 0, sizeof(*pbuffer));
    eglInitRefCount(&pbuffer->surface.refCount);
    pbuffer->surface.display = display;
    pbuffer->surface.configID = config;
    pbuffer->surface.colorBuffer = colorBuffer;
    pbuffer->surface.colorBufferStride = bytesPerPixel * width;
    pbuffer->surface.width = width;
    pbuffer->surface.height = height;
    pbuffer->surface.id = ++display->surfaceCount;
    pbuffer->surface.type = EGL_SURFACE_PBUFFER;
    pbuffer->isLargestPbuffer = largestPbuffer;
    pbuffer->mipmapTexture = mipmapTexture;
    pbuffer->textureFormat = textureFormat;
    pbuffer->textureTarget = textureTarget;
    pbuffer->mipmapLevel = 0;

    if (eglUpdateSurfaceBuffers(&pbuffer->surface) == 0)
    {
        if (pbuffer->surface.colorBuffer != NULL)
            eglFree(pbuffer->surface.colorBuffer);
        if (pbuffer->surface.depthBuffer != NULL)
            eglFree(pbuffer->surface.depthBuffer);
        if (pbuffer->surface.stencilBuffer != NULL)
            eglFree(pbuffer->surface.stencilBuffer);
        eglFree(pbuffer);
        eglSetError(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }

    eglDisplayAddSurface(display, &pbuffer->surface);
    return pbuffer->surface.id;
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list)
{
    eglDisplay_s *display;
    eglContext_s *shareCtx = NULL;
    eglContext_s *ctx;
    oglContext_s *shareEsCtx = NULL;
    oglContext_s *memShareEsCtx = NULL;
    oglContext_s *esCtx;

    if (egliGetState() == NULL)
    {
        eglSetError(EGL_NOT_INITIALIZED);
        return EGL_NO_CONTEXT;
    }

    eglSetError(EGL_SUCCESS);
    display = eglMapDisplay(dpy);
    if (display == NULL)
    {
        eglSetError(EGL_BAD_DISPLAY);
        return EGL_NO_CONTEXT;
    }

    if (eglIsValidConfigID(config) == 0)
    {
        eglSetError(EGL_BAD_CONFIG);
        return EGL_NO_CONTEXT;
    }

    if (share_list != EGL_NO_CONTEXT)
    {
        shareCtx = eglMapContext(display, share_list);
        if (shareCtx == NULL)
        {
            eglSetError(EGL_BAD_CONTEXT);
            return EGL_NO_CONTEXT;
        }
    }

    if (eglIsEmptyList(attrib_list) == 0)
    {
        eglSetError(EGL_BAD_ATTRIBUTE);
        return EGL_NO_CONTEXT;
    }

    if (shareCtx != NULL)
        shareEsCtx = shareCtx->esContext;

    ctx = (eglContext_s *)eglAlloc(sizeof(eglContext_s));
    if (ctx == NULL)
    {
        eglSetError(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }

    if (display->contextList != NULL)
        memShareEsCtx = display->contextList->esContext;

    esCtx = oglCreateContext(shareEsCtx, memShareEsCtx);
    if (esCtx == NULL)
    {
        eglFree(ctx);
        eglSetError(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }

    eglInitRefCount(&ctx->refCount);
    ctx->display = display;
    ctx->esContext = esCtx;
    ctx->configID = config;
    ctx->id = ++display->contextCount;
    ctx->hasBeenBound = 0;

    eglDisplayAddContext(display, ctx);
    return ctx->id;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    eglDisplay_s *display;
    eglContext_s *context;

    if (egliGetState() == NULL)
    {
        eglSetError(EGL_NOT_INITIALIZED);
        return EGL_FALSE;
    }

    eglSetError(EGL_SUCCESS);
    display = eglMapDisplay(dpy);
    context = eglMapContext(display, ctx);
    if (display == NULL)
    {
        eglSetError(EGL_BAD_DISPLAY);
        return EGL_FALSE;
    }
    if (context == NULL)
    {
        eglSetError(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }

    if (eglDestroyRef(&context->refCount) != 0)
    {
        eglSurface_s *surf;
        for (surf = display->surfaceList; surf != NULL; surf = surf->next)
        {
            if (surf->type == EGL_SURFACE_PBUFFER)
            {
                eglPbufferSurface *pbuf = (eglPbufferSurface *)surf;
                if (pbuf->boundESContext == context->esContext)
                {
                    if (pbuf->textureName != 0 &&
                        oglIsSurfaceTextureReleased(pbuf->boundESContext, pbuf->textureName) == 0)
                    {
                        oglReleaseSurfaceTexture(pbuf->boundESContext, pbuf->textureName, pbuf->mipmapLevel);
                    }
                    pbuf->boundESContext = NULL;
                    pbuf->textureName = 0;
                }
            }
        }
        eglFreeContext(context);
    }

    return EGL_TRUE;
}

int eglCheckSurface(eglSurface_s *surface)
{
    if (surface->type == EGL_SURFACE_WINDOW)
    {
        if (egliIsValidNativeWindow(((eglWindowSurface *)surface)->nativeWindow) == 0)
        {
            eglSetError(EGL_BAD_NATIVE_WINDOW);
            return 0;
        }
    }
    else if (surface->type == EGL_SURFACE_PIXMAP)
    {
        if (egliIsValidNativePixmap(((eglPixmapSurface *)surface)->nativePixmap) == 0)
        {
            eglSetError(EGL_BAD_NATIVE_PIXMAP);
            return 0;
        }
    }
    return 1;
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    eglState_s *state = egliGetState();
    eglDisplay_s *display;
    eglContext_s *context;
    eglSurface_s *drawSurface;
    eglSurface_s *readSurface;
    eglContext_s *prevContext;
    eglSurface_s *prevDrawSurface;
    eglSurface_s *prevReadSurface;
    int firstBind = 0;

    if (state == NULL)
    {
        eglSetError(EGL_NOT_INITIALIZED);
        return EGL_FALSE;
    }

    eglSetError(EGL_SUCCESS);
    EGL_ASSERT_AT("eglApi.inl", 279, "eglIsValidState(state)", eglIsValidState(state) != 0);

    display = eglMapDisplay(dpy);
    context = eglMapContext(display, ctx);
    drawSurface = eglMapSurface(display, draw);
    readSurface = eglMapSurface(display, read);

    if (display == NULL)
    {
        eglSetError(EGL_BAD_DISPLAY);
        return EGL_FALSE;
    }
    if (ctx != EGL_NO_CONTEXT && context == NULL)
    {
        eglSetError(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }
    if ((draw != EGL_NO_SURFACE && drawSurface == NULL) ||
        (read != EGL_NO_SURFACE && readSurface == NULL))
    {
        eglSetError(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }

    if (context == NULL)
    {
        if (drawSurface != NULL || readSurface != NULL)
        {
            eglSetError(EGL_BAD_MATCH);
            return EGL_FALSE;
        }
    }
    else
    {
        if (drawSurface == NULL || readSurface == NULL ||
            drawSurface->configID != context->configID ||
            readSurface->configID != context->configID)
        {
            eglSetError(EGL_BAD_MATCH);
            return EGL_FALSE;
        }

        if (drawSurface->type == EGL_SURFACE_PBUFFER)
        {
            eglPbufferSurface *pbuf = (eglPbufferSurface *)drawSurface;
            if (pbuf->boundESContext != NULL &&
                oglIsSurfaceTextureReleased(pbuf->boundESContext, pbuf->textureName) != 0)
            {
                pbuf->boundESContext = NULL;
                pbuf->textureName = 0;
            }
            if (pbuf->boundESContext != NULL)
            {
                eglSetError(EGL_BAD_SURFACE);
                return EGL_FALSE;
            }
        }
        if (readSurface->type == EGL_SURFACE_PBUFFER)
        {
            eglPbufferSurface *pbuf = (eglPbufferSurface *)readSurface;
            if (pbuf->boundESContext != NULL &&
                oglIsSurfaceTextureReleased(pbuf->boundESContext, pbuf->textureName) != 0)
            {
                pbuf->boundESContext = NULL;
                pbuf->textureName = 0;
            }
            if (pbuf->boundESContext != NULL)
            {
                eglSetError(EGL_BAD_SURFACE);
                return EGL_FALSE;
            }
        }

        if (eglCheckSurface(drawSurface) == 0 || eglCheckSurface(readSurface) == 0)
            return EGL_FALSE;

        if ((context->refCount.referenceCount != 0 && state->currentContext != context) ||
            (drawSurface->refCount.referenceCount != 0 &&
             state->currentDrawSurface != drawSurface &&
             state->currentReadSurface != drawSurface) ||
            (readSurface->refCount.referenceCount != 0 &&
             state->currentDrawSurface != readSurface &&
             state->currentReadSurface != readSurface))
        {
            eglSetError(EGL_BAD_ACCESS);
            return EGL_FALSE;
        }
    }

    prevContext = state->currentContext;
    prevDrawSurface = state->currentDrawSurface;
    prevReadSurface = state->currentReadSurface;

    if (prevContext != NULL)
    {
        EGL_ASSERT_AT("eglApi.inl", 336, "prevReadSurface && prevDrawSurface",
                      prevReadSurface != NULL && prevDrawSurface != NULL);
        glFlush();
        glFinish();
    }

    if (context != NULL)
    {
        EGL_ASSERT_AT("eglApi.inl", 344, "drawSurface && readSurface",
                      drawSurface != NULL && readSurface != NULL);
        firstBind = (context->hasBeenBound == 0);
        if (eglUpdateSurfaces(context->esContext, drawSurface, readSurface) == 0)
        {
            eglSetError(EGL_BAD_ALLOC);
            return EGL_FALSE;
        }
    }

    if (context != NULL)
    {
        eglAddRef(&context->refCount);
        eglAddRef(&drawSurface->refCount);
        eglAddRef(&readSurface->refCount);
        state->currentESContext = context->esContext;
        state->currentContext = context;
        state->currentDrawSurface = drawSurface;
        state->currentReadSurface = readSurface;
        state->currentDisplay = display;
        context->hasBeenBound = 1;
    }
    else
    {
        state->currentESContext = NULL;
        state->currentContext = NULL;
        state->currentDrawSurface = NULL;
        state->currentReadSurface = NULL;
        state->currentDisplay = NULL;
    }

    if (prevContext != NULL)
    {
        eglContextReleaseRef(prevContext);
        eglSurfaceReleaseRef(prevDrawSurface);
        eglSurfaceReleaseRef(prevReadSurface);
    }

    if (firstBind != 0)
    {
        glViewport(0, 0, drawSurface->width, drawSurface->height);
        glScissor(0, 0, drawSurface->width, drawSurface->height);
    }

    EGL_ASSERT_AT("eglApi.inl", 392, "eglIsValidState(state)", eglIsValidState(state) != 0);
    return EGL_TRUE;
}
