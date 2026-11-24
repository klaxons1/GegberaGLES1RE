/* oglBufObjManager.c - OpenGL ES VBO name & hash-table manager
 * (port of oglBufObjManager.inl). */
#include <stdint.h>
#include "gerbera/api_opengl/oglContext.h"

int oglIsValidBufObj(oglBufObj_s *bufObj)
{
    OGL_ASSERT_AT("oglBufObjManager.inl", 43, "bufObj", bufObj != NULL);
    OGL_ASSERT_AT("oglBufObjManager.inl", 44,
                  "hgInRange32(bufObj->usage, 0, OGL_BUFOBJUSAGE_MAX - 1)",
                  hgInRange32((int)bufObj->usage, 0, OGL_BUFOBJUSAGE_MAX - 1) != 0);
    return 1;
}

int oglIsValidBufObjManager(oglBufObjManager_s *bufObjManager)
{
    int capacity;
    int numExists;
    int numDeleted;
    int i;

    OGL_ASSERT_AT("oglBufObjManager.inl", 57, "bufObjManager", bufObjManager != NULL);
    capacity   = bufObjManager->capacity;
    numExists  = 0;
    numDeleted = 0;
    for (i = 0; i < capacity; ++i)
    {
        oglBufObjContainer_s *bufObjContainer = &bufObjManager->containers[i];
        OGL_ASSERT_AT("oglBufObjManager.inl", 68,
                      "hgInRange32(bufObjContainer->state, 0, OGL_BUFOBJSTATE_MAX)",
                      hgInRange32((int)bufObjContainer->state, 0, OGL_BUFOBJSTATE_MAX) != 0);
        if (bufObjContainer->state == OGL_BUFOBJSTATE_VALID)
        {
            OGL_ASSERT_AT("oglBufObjManager.inl", 71,
                          "oglIsValidBufObj(bufObjContainer->object)",
                          oglIsValidBufObj(bufObjContainer->object) != 0);
            ++numExists;
        }
        else if (bufObjContainer->state == OGL_BUFOBJSTATE_DELETED)
        {
            ++numDeleted;
        }
    }
    OGL_ASSERT_AT("oglBufObjManager.inl", 78,
                  "numExists == bufObjManager->numObjects",
                  numExists == bufObjManager->numObjects);
    OGL_ASSERT_AT("oglBufObjManager.inl", 79,
                  "numDeleted == bufObjManager->numDeleted",
                  numDeleted == bufObjManager->numDeleted);
    return 1;
}

int oglHashBufObjIndex(unsigned int bufObjIndex)
{
    return (int)(bufObjIndex ^ (8u * bufObjIndex) ^ (bufObjIndex >> 7));
}

int oglHashBufObjIndex2(unsigned int bufObjIndex)
{
    return (int)(~(bufObjIndex ^ (2u * bufObjIndex) ^ (bufObjIndex >> 5)) | 1u);
}

void oglBufObjHashInsert(oglBufObjManager_s *bufObjManager, unsigned int bufObjIndex, oglBufObj_s *object)
{
    int capacity;
    /* The original 32-bit probe additions wrap for arbitrary GLuint names. */
    unsigned int idx;
    unsigned int step;
    oglBufObjContainer_s *c;

    OGL_ASSERT_AT("oglBufObjManager.inl", 121, "object", object != NULL);
    OGL_ASSERT_AT("oglBufObjManager.inl", 122,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    OGL_ASSERT_AT("oglBufObjManager.inl", 123,
                  "bufObjManager->numObjects < bufObjManager->capacity",
                  bufObjManager->numObjects < bufObjManager->capacity);
    capacity = bufObjManager->capacity;
    idx      = oglHashBufObjIndex(bufObjIndex);
    step     = oglHashBufObjIndex2(bufObjIndex);
    do
    {
        c = &bufObjManager->containers[idx & (capacity - 1)];
        idx += step;
    }
    while (c->state == OGL_BUFOBJSTATE_VALID);

    if (c->state == OGL_BUFOBJSTATE_DELETED)
        --bufObjManager->numDeleted;
    c->state       = OGL_BUFOBJSTATE_VALID;
    c->bufObjIndex = bufObjIndex;
    c->object      = object;
    ++bufObjManager->numObjects;
}

void oglBufObjHashRemove(oglBufObjManager_s *bufObjManager, unsigned int bufObjIndex)
{
    int capacity;
    unsigned int idx;
    unsigned int step;
    oglBufObjContainer_s *c;

    OGL_ASSERT_AT("oglBufObjManager.inl", 159,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    OGL_ASSERT_AT("oglBufObjManager.inl", 160,
                  "bufObjManager->numObjects > 0",
                  bufObjManager->numObjects > 0);
    capacity = bufObjManager->capacity;
    idx      = oglHashBufObjIndex(bufObjIndex);
    step     = oglHashBufObjIndex2(bufObjIndex);
    for (;;)
    {
        c = &bufObjManager->containers[idx & (capacity - 1)];
        idx += step;
        if (c->state == OGL_BUFOBJSTATE_VALID && c->bufObjIndex == bufObjIndex)
            break;
        if (c->state == OGL_BUFOBJSTATE_EMPTY)
            return;
    }
    c->state  = OGL_BUFOBJSTATE_DELETED;
    c->object = (oglBufObj_s *)(uintptr_t)0xACDCACDCu;
    ++bufObjManager->numDeleted;
    --bufObjManager->numObjects;
}

oglBufObj_s *oglGetBufObj(oglBufObjManager_s *bufObjManager, unsigned int bufObjIndex)
{
    OGL_ASSERT_AT("oglBufObjManager.inl", 198,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    if (bufObjManager->numObjects > 0)
    {
        int capacity = bufObjManager->capacity;
        unsigned int idx  = (unsigned int)oglHashBufObjIndex(bufObjIndex);
        unsigned int step = (unsigned int)oglHashBufObjIndex2(bufObjIndex);
        int i;

        for (i = 0; i < capacity; ++i)
        {
            oglBufObjContainer_s *c = &bufObjManager->containers[idx & (capacity - 1)];
            idx += step;
            if (c->state == OGL_BUFOBJSTATE_VALID && c->bufObjIndex == bufObjIndex)
                return c->object;
            if (c->state == OGL_BUFOBJSTATE_EMPTY)
                return NULL;
        }
    }
    return NULL;
}

int oglIsBufObj(oglBufObjManager_s *bufObjManager, unsigned int bufObjIndex)
{
    return oglGetBufObj(bufObjManager, bufObjIndex) != NULL;
}

void oglGrowBufObjHash(oglBufObjManager_s *bufObjManager)
{
    int numObjects;
    int numDeleted;
    int oldCapacity;

    OGL_ASSERT_AT("oglBufObjManager.inl", 261,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    numObjects  = bufObjManager->numObjects;
    numDeleted  = bufObjManager->numDeleted;
    oldCapacity = bufObjManager->capacity;
    OGL_ASSERT_AT("oglBufObjManager.inl", 267,
                  "numObjects <= oldCapacity",
                  numObjects <= oldCapacity);
    if (((3 * (numDeleted + numObjects)) >> 1) >= oldCapacity)
    {
        int newCapacity = hgMax32(hgMin32(oglNextPowerOfTwo(2 * numObjects), 2 * oldCapacity), 8);
        oglBufObjContainer_s *newContainers =
            (oglBufObjContainer_s *)eglAlloc((unsigned int)((size_t)newCapacity * sizeof(oglBufObjContainer_s)));
        if (newContainers != NULL)
        {
            int i;
            for (i = 0; i < newCapacity; ++i)
                newContainers[i].state = OGL_BUFOBJSTATE_EMPTY;
            if (oldCapacity != 0)
            {
                oglBufObjContainer_s *oldContainers = bufObjManager->containers;
                OGL_ASSERT_AT("oglBufObjManager.inl", 289,
                              "oldContainers",
                              oldContainers != NULL);
                if (bufObjManager->numDeleted != 0)
                    bufObjManager->lastGenerated = 0;
                bufObjManager->capacity   = newCapacity;
                bufObjManager->containers = newContainers;
                bufObjManager->numObjects = 0;
                bufObjManager->numDeleted = 0;
                for (i = 0; i < oldCapacity; ++i)
                {
                    oglBufObjContainer_s *c = &oldContainers[i];
                    if (c->state == OGL_BUFOBJSTATE_VALID)
                        oglBufObjHashInsert(bufObjManager, c->bufObjIndex, c->object);
                }
                eglFree(oldContainers);
            }
            bufObjManager->containers = newContainers;
            bufObjManager->capacity   = newCapacity;
            OGL_ASSERT_AT("oglBufObjManager.inl", 313,
                          "oglIsValidBufObjManager(bufObjManager)",
                          oglIsValidBufObjManager(bufObjManager) != 0);
        }
    }
}

unsigned int oglGenerateBufObjName(oglBufObjManager_s *bufObjManager)
{
    int nextIndex;

    OGL_ASSERT_AT("oglBufObjManager.inl", 329,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    nextIndex = bufObjManager->lastGenerated;
    while (oglIsBufObj(bufObjManager, (unsigned int)nextIndex) != 0)
        ++nextIndex;
    bufObjManager->lastGenerated = nextIndex;
    return (unsigned int)nextIndex;
}

void oglInitBufObj(oglBufObj_s *bufObj)
{
    OGL_ASSERT_AT("oglBufObjManager.inl", 351, "bufObj", bufObj != NULL);
    bufObj->size   = 0;
    bufObj->usage  = OGL_BUFOBJUSAGE_STATIC_DRAW;
    bufObj->buffer = NULL;
}

void oglFreeBufObjData(oglBufObj_s *bufObj)
{
    OGL_ASSERT_AT("oglBufObjManager.inl", 366, "bufObj", bufObj != NULL);
    if (bufObj->buffer != NULL)
    {
        eglFree(bufObj->buffer);
        bufObj->buffer = NULL;
    }
    bufObj->size  = 0;
    bufObj->usage = OGL_BUFOBJUSAGE_MAX;
}

int oglAllocateBufObj(oglBufObjManager_s *bufObjManager, unsigned int bufObjIndex)
{
    oglBufObj_s *bufObj;

    OGL_ASSERT_AT("oglBufObjManager.inl", 389,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    if (bufObjManager->numObjects >= 10000)
        return 0;
    oglGrowBufObjHash(bufObjManager);
    bufObj = (oglBufObj_s *)eglAlloc((unsigned int)sizeof(oglBufObj_s));
    if (bufObj == NULL)
        return 0;
    OGL_ASSERT_AT("oglBufObjManager.inl", 404,
                  "bufObjManager->numObjects <= bufObjManager->capacity",
                  bufObjManager->numObjects <= bufObjManager->capacity);
    if (bufObjManager->numObjects == bufObjManager->capacity)
    {
        eglFree(bufObj);
        return 0;
    }
    oglInitBufObj(bufObj);
    oglBufObjHashInsert(bufObjManager, bufObjIndex, bufObj);
    OGL_ASSERT_AT("oglBufObjManager.inl", 416,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    return 1;
}

void oglDeleteBufObj(oglBufObjManager_s *bufObjManager, unsigned int bufObjIndex)
{
    OGL_ASSERT_AT("oglBufObjManager.inl", 431, "bufObjManager", bufObjManager != NULL);
    OGL_ASSERT_AT("oglBufObjManager.inl", 432,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
    if (oglIsBufObj(bufObjManager, bufObjIndex) != 0)
    {
        oglBufObj_s *bufObj = oglGetBufObj(bufObjManager, bufObjIndex);
        oglBufObjHashRemove(bufObjManager, bufObjIndex);
        oglFreeBufObjData(bufObj);
        eglFree(bufObj);
    }
    OGL_ASSERT_AT("oglBufObjManager.inl", 444,
                  "oglIsValidBufObjManager(bufObjManager)",
                  oglIsValidBufObjManager(bufObjManager) != 0);
}

oglBufObjManager_s *oglCreateBufObjManager(void)
{
    oglBufObjManager_s *bufObjManager =
        (oglBufObjManager_s *)eglAlloc((unsigned int)sizeof(oglBufObjManager_s));
    if (bufObjManager == NULL)
        return NULL;
    hgMemSet(bufObjManager, 0, (int)sizeof(oglBufObjManager_s));
    if (oglAllocateBufObj(bufObjManager, 0) != 0)
        return bufObjManager;
    eglFree(bufObjManager);
    return NULL;
}

void oglDestroyBufObjManager(oglBufObjManager_s *bufObjManager)
{
    int i;

    if (bufObjManager != NULL)
    {
        for (i = 0; i < bufObjManager->capacity; ++i)
        {
            oglBufObjContainer_s *c = &bufObjManager->containers[i];
            if (c->state == OGL_BUFOBJSTATE_VALID)
                oglDeleteBufObj(bufObjManager, c->bufObjIndex);
            OGL_ASSERT_AT("oglBufObjManager.inl", 498,
                          "c->state != OGL_BUFOBJSTATE_VALID",
                          c->state != OGL_BUFOBJSTATE_VALID);
        }
        for (i = 0; i < bufObjManager->capacity; ++i)
        {
            OGL_ASSERT_AT("oglBufObjManager.inl", 507,
                          "bufObjManager->containers[i].state != OGL_BUFOBJSTATE_VALID",
                          bufObjManager->containers[i].state != OGL_BUFOBJSTATE_VALID);
        }
        eglFree(bufObjManager->containers);
        eglFree(bufObjManager);
    }
}

int oglSetupBufObj(oglBufObj_s *bufObj, OGL_BUFOBJUSAGE usage, int size)
{
    void *buffer;

    OGL_ASSERT_AT("oglBufObjManager.inl", 530, "bufObj", bufObj != NULL);
    OGL_ASSERT_AT("oglBufObjManager.inl", 531, "size >= 0", size >= 0);
    OGL_ASSERT_AT("oglBufObjManager.inl", 532,
                  "hgInRange32(usage, 0, OGL_BUFOBJUSAGE_MAX - 1)",
                  hgInRange32((int)usage, 0, OGL_BUFOBJUSAGE_MAX - 1) != 0);
    buffer = NULL;
    if (size > 0)
    {
        buffer = eglAlloc((unsigned int)size);
        if (buffer == NULL)
            return 0;
    }
    if (bufObj->buffer != NULL)
    {
        OGL_ASSERT_AT("oglBufObjManager.inl", 545, "bufObj->size > 0", bufObj->size > 0);
        eglFree(bufObj->buffer);
    }
    bufObj->usage  = usage;
    bufObj->size   = size;
    bufObj->buffer = buffer;
    return 1;
}

oglBufObj_s *oglGetActiveBufObj(oglContext_s *context, OGL_BUFOBJTARGET bufObjTarget)
{
    OGL_ASSERT_AT("oglBufObjManager.inl", 567, "context", context != NULL);
    OGL_ASSERT_AT("oglBufObjManager.inl", 568,
                  "hgInRange32(bufObjTarget, OGL_BUFOBJTARGET_ARRAY, OGL_BUFOBJTARGET_MAX - 1)",
                  hgInRange32((int)bufObjTarget, OGL_BUFOBJTARGET_ARRAY, OGL_BUFOBJTARGET_MAX - 1) != 0);
    return oglGetBufObj(context->bufObjManager, context->activeBufObjs[bufObjTarget]);
}

void oglBindBuffer(oglContext_s *context, OGL_BUFOBJTARGET bufObjTarget, unsigned int bufObjIndex)
{
    unsigned int pname;
    int val;

    OGL_ASSERT_AT("oglBufObjManager.inl", 586, "context", context != NULL);
    OGL_ASSERT_AT("oglBufObjManager.inl", 587,
                  "hgInRange32((int)bufObjTarget, 0, OGL_BUFOBJTARGET_MAX - 1)",
                  hgInRange32((int)bufObjTarget, 0, OGL_BUFOBJTARGET_MAX - 1) != 0);
    OGL_ASSERT_AT("oglBufObjManager.inl", 588,
                  "oglIsBufObj(context->bufObjManager, bufObjIndex)",
                  oglIsBufObj(context->bufObjManager, bufObjIndex) != 0);
    context->activeBufObjs[bufObjTarget] = bufObjIndex;
    pname = (bufObjTarget != OGL_BUFOBJTARGET_ARRAY)
            ? GL_ELEMENT_ARRAY_BUFFER_BINDING
            : GL_ARRAY_BUFFER_BINDING;
    val = (int)bufObjIndex;
    oglSetGetterValueInt(context, 0, pname, &val);
}
