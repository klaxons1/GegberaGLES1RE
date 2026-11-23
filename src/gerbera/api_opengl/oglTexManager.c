/* oglTexManager.c - OpenGL ES texture name & hash-table manager
 * (port of oglTexManager.inl). */
#include <stdint.h>
#include "gerbera/api_opengl/oglContext.h"

int oglHashTexIndex(unsigned int texIndex)
{
    return (int)(texIndex ^ (8u * texIndex) ^ (texIndex >> 7));
}

int oglHashTexIndex2(unsigned int texIndex)
{
    return (int)(~(texIndex ^ (2u * texIndex) ^ (texIndex >> 5)) | 1u);
}

int oglIsValidTextureManager(oglTextureManager *texManager)
{
    int capacity;
    int numExists;
    int numDeleted;
    int i;

    OGL_ASSERT_AT("oglTexManager.inl", 38, "texManager", texManager != NULL);
    capacity   = texManager->capacity;
    numExists  = 0;
    numDeleted = 0;
    OGL_ASSERT_AT("oglTexManager.inl", 45,
                  "texManager->containers ? capacity : !capacity",
                  (texManager->containers != NULL ? capacity : (capacity == 0)) != 0);
    for (i = 0; i < capacity; ++i)
    {
        oglTexContainer_s *texContainer = &texManager->containers[i];
        OGL_ASSERT_AT("oglTexManager.inl", 51,
                      "hgInRange32(texContainer->state, 0, OGL_TEXSTATE_MAX)",
                      hgInRange32((int)texContainer->state, 0, OGL_TEXSTATE_MAX) != 0);
        if (texContainer->state == OGL_TEXSTATE_VALID)
        {
            grGetTextureTarget(texContainer->texture);
            ++numExists;
        }
        else if (texContainer->state == OGL_TEXSTATE_DELETED)
        {
            ++numDeleted;
        }
    }
    OGL_ASSERT_AT("oglTexManager.inl", 62,
                  "numExists == texManager->numTextures",
                  numExists == texManager->numTextures);
    OGL_ASSERT_AT("oglTexManager.inl", 63,
                  "numDeleted == texManager->numDeleted",
                  numDeleted == texManager->numDeleted);
    return 1;
}

void oglTexHashInsert(oglTextureManager *texManager, unsigned int texIndex, _grTexture *texture)
{
    int capacity;
    int ndx;
    int step;
    oglTexContainer_s *c;

    OGL_ASSERT_AT("oglTexManager.inl", 106, "texture", texture != NULL);
    OGL_ASSERT_AT("oglTexManager.inl", 107,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    OGL_ASSERT_AT("oglTexManager.inl", 108,
                  "texManager->numTextures < texManager->capacity",
                  texManager->numTextures < texManager->capacity);
    capacity = texManager->capacity;
    ndx      = oglHashTexIndex(texIndex);
    step     = oglHashTexIndex2(texIndex);
    do
    {
        c = &texManager->containers[ndx & (capacity - 1)];
        ndx += step;
    }
    while (c->state == OGL_TEXSTATE_VALID);

    if (c->state == OGL_TEXSTATE_DELETED)
        --texManager->numDeleted;
    c->state    = OGL_TEXSTATE_VALID;
    c->texIndex = texIndex;
    c->texture  = texture;
    ++texManager->numTextures;
}

void oglTexHashRemove(oglTextureManager *texManager, unsigned int texIndex)
{
    int capacity;
    int ndx;
    int step;
    oglTexContainer_s *c;

    OGL_ASSERT_AT("oglTexManager.inl", 144,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    OGL_ASSERT_AT("oglTexManager.inl", 145,
                  "texManager->numTextures > 0",
                  texManager->numTextures > 0);
    capacity = texManager->capacity;
    ndx      = oglHashTexIndex(texIndex);
    step     = oglHashTexIndex2(texIndex);
    for (;;)
    {
        c = &texManager->containers[ndx & (capacity - 1)];
        ndx += step;
        if (c->state == OGL_TEXSTATE_VALID && c->texIndex == texIndex)
            break;
        if (c->state == OGL_TEXSTATE_EMPTY)
            return;
    }
    c->state   = OGL_TEXSTATE_DELETED;
    c->texture = (_grTexture *)(uintptr_t)0xACDCACDCu;
    ++texManager->numDeleted;
    --texManager->numTextures;
}

_grTexture *oglGetTexture(oglTextureManager *texManager, unsigned int texIndex)
{
    OGL_ASSERT_AT("oglTexManager.inl", 183,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    if (texManager->numTextures > 0)
    {
        int capacity = texManager->capacity;
        int ndx      = oglHashTexIndex(texIndex);
        int step     = oglHashTexIndex2(texIndex);
        int i;

        for (i = 0; i < capacity; ++i)
        {
            oglTexContainer_s *c = &texManager->containers[ndx & (capacity - 1)];
            ndx += step;
            if (c->state == OGL_TEXSTATE_VALID && c->texIndex == texIndex)
                return c->texture;
            if (c->state == OGL_TEXSTATE_EMPTY)
                return NULL;
        }
    }
    return NULL;
}

int oglIsTexture(oglTextureManager *texManager, unsigned int texIndex)
{
    return oglGetTexture(texManager, texIndex) != NULL;
}

int oglNextPowerOfTwo(int v)
{
    return 1 << (32 - hgClz32((HGuint32)v));
}

void oglGrowTextureHash(oglTextureManager *texManager)
{
    int numTextures;
    int numDeleted;
    int oldCapacity;

    OGL_ASSERT_AT("oglTexManager.inl", 234,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    numTextures = texManager->numTextures;
    numDeleted  = texManager->numDeleted;
    oldCapacity = texManager->capacity;
    OGL_ASSERT_AT("oglTexManager.inl", 240,
                  "numTextures <= oldCapacity",
                  numTextures <= oldCapacity);
    if (((3 * (numDeleted + numTextures)) >> 1) >= oldCapacity)
    {
        int newCapacity = hgMax32(hgMin32(oglNextPowerOfTwo(2 * numTextures), 2 * oldCapacity), 8);
        oglTexContainer_s *newContainers =
            (oglTexContainer_s *)eglAlloc((unsigned int)((size_t)newCapacity * sizeof(oglTexContainer_s)));
        if (newContainers != NULL)
        {
            int i;
            for (i = 0; i < newCapacity; ++i)
                newContainers[i].state = OGL_TEXSTATE_EMPTY;
            if (oldCapacity != 0)
            {
                oglTexContainer_s *oldContainers = texManager->containers;
                OGL_ASSERT_AT("oglTexManager.inl", 265,
                              "oldContainers",
                              oldContainers != NULL);
                if (texManager->numDeleted != 0)
                    texManager->lastGenerated = 0;
                texManager->capacity    = newCapacity;
                texManager->containers  = newContainers;
                texManager->numTextures = 0;
                texManager->numDeleted  = 0;
                for (i = 0; i < oldCapacity; ++i)
                {
                    oglTexContainer_s *c = &oldContainers[i];
                    if (c->state == OGL_TEXSTATE_VALID)
                        oglTexHashInsert(texManager, c->texIndex, c->texture);
                }
                eglFree(oldContainers);
            }
            texManager->containers = newContainers;
            texManager->capacity   = newCapacity;
            OGL_ASSERT_AT("oglTexManager.inl", 289,
                          "oglIsValidTextureManager(texManager)",
                          oglIsValidTextureManager(texManager) != 0);
        }
    }
}

unsigned int oglGenerateTextureName(oglTextureManager *texManager)
{
    int nextIndex;

    OGL_ASSERT_AT("oglTexManager.inl", 305,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    nextIndex = texManager->lastGenerated;
    while (oglIsTexture(texManager, (unsigned int)nextIndex) != 0)
        ++nextIndex;
    texManager->lastGenerated = nextIndex;
    return (unsigned int)nextIndex;
}

int oglAllocateTexture(oglTextureManager *texManager, unsigned int texIndex)
{
    _grTexture *texture;

    OGL_ASSERT_AT("oglTexManager.inl", 328,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    if (texManager->numTextures >= 10000)
        return 0;
    oglGrowTextureHash(texManager);
    texture = (_grTexture *)eglAlloc((unsigned int)sizeof(_grTexture));
    if (texture == NULL)
        return 0;
    OGL_ASSERT_AT("oglTexManager.inl", 343,
                  "texManager->numTextures <= texManager->capacity",
                  texManager->numTextures <= texManager->capacity);
    if (texManager->numTextures == texManager->capacity)
    {
        eglFree(texture);
        return 0;
    }
    grInitTexture(texture);
    oglTexHashInsert(texManager, texIndex, texture);
    OGL_ASSERT_AT("oglTexManager.inl", 355,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    return 1;
}

void oglDeleteTexture(oglTextureManager *texManager, unsigned int texIndex)
{
    OGL_ASSERT_AT("oglTexManager.inl", 370, "texManager", texManager != NULL);
    OGL_ASSERT_AT("oglTexManager.inl", 371,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
    if (oglIsTexture(texManager, texIndex) != 0)
    {
        _grTexture *texture = oglGetTexture(texManager, texIndex);
        oglTexHashRemove(texManager, texIndex);
        grrFreeTextureData(texture);
        eglFree(texture);
    }
    OGL_ASSERT_AT("oglTexManager.inl", 383,
                  "oglIsValidTextureManager(texManager)",
                  oglIsValidTextureManager(texManager) != 0);
}

void oglDestroyTextureManager(oglTextureManager *texManager)
{
    int i;

    if (texManager != NULL)
    {
        for (i = 0; i < texManager->capacity; ++i)
        {
            oglTexContainer_s *c = &texManager->containers[i];
            if (c->state == OGL_TEXSTATE_VALID)
                oglDeleteTexture(texManager, c->texIndex);
            OGL_ASSERT_AT("oglTexManager.inl", 409,
                          "c->state != OGL_TEXSTATE_VALID",
                          c->state != OGL_TEXSTATE_VALID);
        }
        for (i = 0; i < texManager->capacity; ++i)
        {
            OGL_ASSERT_AT("oglTexManager.inl", 418,
                          "texManager->containers[i].state != OGL_TEXSTATE_VALID",
                          texManager->containers[i].state != OGL_TEXSTATE_VALID);
        }
        eglFree(texManager->containers);
        eglFree(texManager);
    }
}

oglTextureManager *oglCreateTextureManager(void)
{
    oglTextureManager *texManager = (oglTextureManager *)eglAlloc((unsigned int)sizeof(oglTextureManager));
    if (texManager == NULL)
        return NULL;
    hgMemSet(texManager, 0, (int)sizeof(oglTextureManager));
    if (oglAllocateTexture(texManager, 0) != 0)
        return texManager;
    oglDestroyTextureManager(texManager);
    return NULL;
}
