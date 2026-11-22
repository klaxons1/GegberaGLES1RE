/* grPipelineCache.c - the compiled-pixel-pipeline program cache (port of
 * grPipelineCache.inl). Keeps the JIT-produced pipeline programs in an arena,
 * keyed by the compressed pipeline signature, with LRU eviction, free-block
 * merging and defragmentation. */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "hg/hgMemory.h"

/* --- grCacheMatchSignatures (grPipelineCache.inl 44; DLL 35328-35342) --------- */

int grCacheMatchSignatures(_grCacheSignature *a, _grCacheSignature *b)
{
    if ((a == NULL || b == NULL) &&
        grAssert("a && b", "grPipelineCache.inl", 44) != 0)
        grDie(0);
    return hgMemCompare(a, b, 0x28) == 0;
}

/* --- grCacheGetOldestUsed (grPipelineCache.inl 58; DLL 35345-35365) ----------- */

_grCacheEntry *grCacheGetOldestUsed(_grPipelineCache *cache)
{
    _grCacheEntry *entry;
    _grCacheEntry *oldest;

    if (cache == NULL && grAssert("cache", "grPipelineCache.inl", 58) != 0)
        grDie(0);
    if (cache->firstUsed == NULL)
        return NULL;
    oldest = cache->firstUsed;
    for (entry = oldest->next; entry != NULL; entry = entry->next)
    {
        if (entry->timeStamp < oldest->timeStamp)
            oldest = entry;
    }
    return oldest;
}

/* --- grCacheChangeList (grPipelineCache.inl 140; DLL 35386-35419) -------------
 * Unlinks entry from *oldList (whose tail it may head) and relinks it at the
 * front of *newList. The decompile's prev[1]/colorsx writes are the standard
 * entry->prev->next / entry->next->prev updates. */

void grCacheChangeList(_grCacheEntry *entry, _grCacheEntry **newList,
                       _grCacheEntry **oldList)
{
    if (entry == NULL && grAssert("entry", "grPipelineCache.inl", 140) != 0)
        grDie(0);
    if (entry->prev != NULL)
    {
        entry->prev->next = entry->next;
    }
    else
    {
        if (*oldList != entry &&
            grAssert("*oldList == entry", "grPipelineCache.inl", 146) != 0)
            grDie(0);
        *oldList = entry->next;
    }
    if (entry->next != NULL)
        entry->next->prev = entry->prev;
    entry->prev = NULL;
    entry->next = *newList;
    if (entry->next != NULL)
        entry->next->prev = entry;
    *newList = entry;
}

/* --- grCacheMergeFreeBlock (grPipelineCache.inl 177; DLL 35422-35459) ---------
 * Two passes: merge entry with an adjacent free block (either side); the
 * surviving record stays on the free list. */

void grCacheMergeFreeBlock(_grPipelineCache *cache, _grCacheEntry *entry)
{
    int i;
    _grCacheEntry *a;

    if ((cache == NULL || entry == NULL) &&
        grAssert("cache && entry", "grPipelineCache.inl", 177) != 0)
        grDie(0);
    for (i = 0; i < 2; ++i)
    {
        for (a = cache->firstFree; a != NULL; a = a->next)
        {
            if (a != entry)
            {
                if (a->dataOffset == entry->dataSize + entry->dataOffset)
                {
                    entry->dataSize += a->dataSize;
                    grCacheChangeList(a, &cache->firstUnused, &cache->firstFree);
                    break;
                }
                if (entry->dataOffset == a->dataSize + a->dataOffset)
                {
                    a->dataSize += entry->dataSize;
                    grCacheChangeList(entry, &cache->firstUnused, &cache->firstFree);
                    entry = a;
                    break;
                }
            }
        }
    }
}

/* --- grCacheRemove (grPipelineCache.inl 223; DLL 35462-35487) ----------------- */

void grCacheRemove(_grPipelineCache *cache, _grCacheEntry *entry)
{
    if ((cache == NULL || entry == NULL) &&
        grAssert("cache && entry", "grPipelineCache.inl", 223) != 0)
        grDie(0);
    grCacheSelfCheck(cache);
    if (entry->dataSize != 0)
    {
        grCacheChangeList(entry, &cache->firstFree, &cache->firstUsed);
        grCacheSelfCheck(cache);
        grCacheMergeFreeBlock(cache, entry);
    }
    else
    {
        grCacheChangeList(entry, &cache->firstUnused, &cache->firstUsed);
    }
    grCacheSelfCheck(cache);
}

/* --- grCacheDefragment (grPipelineCache.inl 248; DLL 35490-35536) -------------
 * While two free blocks exist: take the lower-offset one, move the used
 * block that follows it down onto it, and merge the remainder. */

void grCacheDefragment(_grPipelineCache *cache)
{
    _grCacheEntry *next;
    _grCacheEntry *usedEntry;
    _grCacheEntry *freeEntry;
    unsigned char *dst;

    if (cache == NULL && grAssert("cache", "grPipelineCache.inl", 248) != 0)
        grDie(0);
    if (cache->firstFree == NULL &&
        grAssert("cache->firstFree", "grPipelineCache.inl", 249) != 0)
        grDie(0);
    while (cache->firstFree->next != NULL)
    {
        if (cache->firstFree->dataOffset >= cache->firstFree->next->dataOffset)
            next = cache->firstFree->next;
        else
            next = cache->firstFree;
        freeEntry = next;
        for (usedEntry = cache->firstUsed;
             usedEntry != NULL &&
             usedEntry->dataOffset != next->dataSize + next->dataOffset;
             usedEntry = usedEntry->next)
        {
        }
        if (usedEntry == NULL &&
            grAssert("usedEntry", "grPipelineCache.inl", 278) != 0)
            grDie(0);
        dst = (unsigned char *)cache->arena + freeEntry->dataOffset;
        hgMemMove(dst, (char *)cache->arena + usedEntry->dataOffset,
                  usedEntry->dataSize);
        grICacheFlush(dst, usedEntry->dataSize);
        usedEntry->dataOffset = freeEntry->dataOffset;
        freeEntry->dataOffset = usedEntry->dataSize + usedEntry->dataOffset;
        grCacheMergeFreeBlock(cache, freeEntry);
        grCacheSelfCheck(cache);
    }
}

/* --- grCacheInit (grPipelineCache.inl 349; DLL 35552-35609) ------------------- */

void grCacheInit(_grPipelineCache *cache, void *arena,
                 _grCacheEntry *entries, int arenaSize, int entryCount)
{
    int i;

    if ((cache == NULL || entries == NULL) &&
        grAssert("cache && entries", "grPipelineCache.inl", 349) != 0)
        grDie(0);
    if ((arenaSize < 0 || entryCount < 0) &&
        grAssert("arenaSize >= 0 && entryCount >= 0",
                 "grPipelineCache.inl", 350) != 0)
        grDie(0);
    if ((arena == NULL && arenaSize != 0) &&
        grAssert("arena || !arenaSize", "grPipelineCache.inl", 351) != 0)
        grDie(0);
    /* the decompile zeroes 0x20 / entryCount << 6 bytes - the 32-bit sizes
     * of cv_anon_172A and _grCacheEntry; hosted pointers are 8 bytes, so
     * zero by sizeof to cover the whole records */
    hgMemSet(cache, 0, sizeof(*cache));
    if (entryCount > 0)
    {
        hgMemSet(entries, 0, entryCount * sizeof(*entries));
        cache->arena = arena;
        cache->entries = entries;
        cache->arenaSize = arenaSize;
        cache->entryCount = entryCount;
        cache->firstFree = entries;
        cache->firstFree->dataSize = arenaSize;
        if (entryCount >= 2)
        {
            cache->firstUnused = entries + 1;
            for (i = 1; i < entryCount - 1; ++i)
                cache->entries[i].next = &cache->entries[i + 1];
        }
    }
    grCacheSelfCheck(cache);
}

/* --- grCacheFind (grPipelineCache.inl 419; DLL 35612-35636) ------------------- */

void *grCacheFind(_grPipelineCache *cache, _grCacheSignature *signature,
                  unsigned int *gradientMask)
{
    _grCacheEntry *e;

    if ((cache == NULL || gradientMask == NULL) &&
        grAssert("cache && gradientMask", "grPipelineCache.inl", 419) != 0)
        grDie(0);
    for (e = cache->firstUsed; e != NULL; e = e->next)
    {
        if (grCacheMatchSignatures(&e->signature, signature) != 0)
        {
            e->timeStamp = ++cache->timeStamp;
            *gradientMask = e->gradientMask;
            return (char *)cache->arena + e->dataOffset;
        }
    }
    return NULL;
}

/* --- grCacheInsert (grPipelineCache.inl 463; DLL 35639-35714) ----------------- */

void *grCacheInsert(_grPipelineCache *cache, _grCacheSignature *signature,
                    unsigned int gradientMask, void *data, int dataSize)
{
    unsigned char *dst;
    int offset;
    _grCacheEntry *newBlock;
    _grCacheEntry *oldest;
    _grCacheEntry *best;

    if ((cache == NULL || signature == NULL || data == NULL) &&
        grAssert("cache && signature && data", "grPipelineCache.inl", 463) != 0)
        grDie(0);
    if (dataSize < 0 &&
        grAssert("dataSize >= 0", "grPipelineCache.inl", 464) != 0)
        grDie(0);
    grCacheSelfCheck(cache);
    if (dataSize > cache->arenaSize || cache->entryCount < 2)
        return NULL;
    while (grCacheGetFreeMemory(cache) < dataSize ||
           cache->firstUnused == NULL || cache->firstFree == NULL)
    {
        oldest = grCacheGetOldestUsed(cache);
        grCacheRemove(cache, oldest);
    }
    best = grCacheFindFreeEntry(cache, dataSize);
    if (best == NULL)
    {
        grCacheDefragment(cache);
        best = grCacheFindFreeEntry(cache, dataSize);
        if (best == NULL &&
            grAssert("best", "grPipelineCache.inl", 503) != 0)
            grDie(0);
    }
    if (cache->firstUnused == NULL &&
        grAssert("cache->firstUnused", "grPipelineCache.inl", 506) != 0)
        grDie(0);
    newBlock = cache->firstUnused;
    offset = best->dataSize - dataSize;
    dst = (unsigned char *)cache->arena + best->dataOffset + offset;
    newBlock->timeStamp = ++cache->timeStamp;
    newBlock->dataOffset = offset + best->dataOffset;
    newBlock->dataSize = dataSize;
    newBlock->gradientMask = gradientMask;
    hgMemCopy(dst, data, dataSize);
    hgMemCopy(newBlock, signature, 0x28);
    grCacheChangeList(newBlock, &cache->firstUsed, &cache->firstUnused);
    best->dataSize = offset;
    if (best->dataSize == 0)
        grCacheChangeList(best, &cache->firstUnused, &cache->firstFree);
    grICacheFlush(dst, dataSize);
    grCacheSelfCheck(cache);
    return dst;
}

/* --- grCacheGetFreeMemory (grSmall.c; DLL 35368-35377) ------------------------ */

int grCacheGetFreeMemory(_grPipelineCache *cache)
{
    _grCacheEntry *entry;
    int totalFree = 0;

    for (entry = cache->firstFree; entry != NULL; entry = entry->next)
        totalFree += entry->dataSize;
    return totalFree;
}

/* --- grCacheSelfCheck (grSmall.c; DLL 35380-35383) ---------------------------- */

void grCacheSelfCheck(_grPipelineCache *cache)
{
    (void)cache;
}

/* --- grCacheFindFreeEntry (grSmall.c; DLL 35539-35549) ------------------------ */

_grCacheEntry *grCacheFindFreeEntry(_grPipelineCache *cache, int dataSize)
{
    _grCacheEntry *entry;

    for (entry = cache->firstFree; entry != NULL; entry = entry->next)
    {
        if (entry->dataSize >= dataSize)
            return entry;
    }
    return NULL;
}
