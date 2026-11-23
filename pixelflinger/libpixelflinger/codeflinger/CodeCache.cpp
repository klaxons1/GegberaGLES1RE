/* libs/pixelflinger/codeflinger/CodeCache.cpp
**
** Symbian OS EKA2 (zero-WSD RChunk::CreateLocalCode + User::IMB_Range with
** out-of-band RAM slot table) and POSIX mmap CodeCache for Codeflinger.
*/

#define LOG_TAG "CodeCache"

#include <stdlib.h>
#include <string.h>
#include <cutils/atomic.h>
#include <cutils/log.h>

#ifdef __SYMBIAN32__
#include "symbian_os.h"
#else
#include <sys/mman.h>
#endif

#include <pf_symbian_tls.h>
#include "CodeCache.h"

/* 32-byte ARM9/ARM11 cache-line alignment for all executable code blocks */
static inline uint32_t pf_align32(size_t n)
{
    return (uint32_t)((n + 31u) & ~(size_t)31u);
}

void pf_exec_arena_init(PfExecArena* arena, void* mem, size_t totalBytes)
{
    memset(arena, 0, sizeof(PfExecArena));
    if (!mem || totalBytes < 64u)
        return;

    uintptr_t raw = (uintptr_t)mem;
    uintptr_t aligned = (raw + 31u) & ~(uintptr_t)31u;
    size_t adj = (size_t)(aligned - raw);
    if (totalBytes <= adj + 64u)
        return;

    arena->base      = (uint8_t*)aligned;
    arena->capacity  = (uint32_t)((totalBytes - adj) & ~(size_t)31u);
    arena->slotCount = 1;
    arena->slots[0].offset = 0;
    arena->slots[0].size   = arena->capacity;
    arena->slots[0].used   = 0;
}

static void pf_coalesce_slots(PfExecArena* arena)
{
    uint32_t i = 0;
    while (i + 1 < arena->slotCount) {
        if (!arena->slots[i].used && !arena->slots[i + 1].used) {
            arena->slots[i].size += arena->slots[i + 1].size;
            uint32_t rem = arena->slotCount - (i + 2);
            if (rem > 0) {
                memmove(&arena->slots[i + 1], &arena->slots[i + 2],
                        rem * sizeof(PfExecSlot));
            }
            arena->slotCount--;
        } else {
            ++i;
        }
    }
}

uint32_t* pf_exec_alloc(PfExecArena* arena, size_t reqBytes)
{
    if (!arena || !arena->base || arena->slotCount == 0)
        return 0;
    uint32_t need = pf_align32(reqBytes ? reqBytes : 32u);
    for (uint32_t i = 0; i < arena->slotCount; ++i) {
        if (!arena->slots[i].used && arena->slots[i].size >= need) {
            uint32_t rem = arena->slots[i].size - need;
            if (rem >= 32u && arena->slotCount < PF_EXEC_MAX_SLOTS) {
                uint32_t tailCnt = arena->slotCount - (i + 1);
                if (tailCnt > 0) {
                    memmove(&arena->slots[i + 2], &arena->slots[i + 1],
                            tailCnt * sizeof(PfExecSlot));
                }
                arena->slots[i + 1].offset = arena->slots[i].offset + need;
                arena->slots[i + 1].size   = rem;
                arena->slots[i + 1].used   = 0;
                arena->slots[i].size       = need;
                arena->slotCount++;
            }
            arena->slots[i].used = 1;
            return (uint32_t*)(arena->base + arena->slots[i].offset);
        }
    }
    return 0;
}

uint32_t* pf_exec_shrink(PfExecArena* arena, void* ptr, size_t newBytes)
{
    if (!arena || !arena->base || !ptr)
        return (uint32_t*)ptr;
    uintptr_t off = (uintptr_t)ptr - (uintptr_t)arena->base;
    if (off >= arena->capacity)
        return (uint32_t*)ptr;

    uint32_t need = pf_align32(newBytes ? newBytes : 32u);
    for (uint32_t i = 0; i < arena->slotCount; ++i) {
        if (arena->slots[i].offset == (uint32_t)off && arena->slots[i].used) {
            if (arena->slots[i].size >= need + 32u && arena->slotCount < PF_EXEC_MAX_SLOTS) {
                uint32_t rem = arena->slots[i].size - need;
                uint32_t tailCnt = arena->slotCount - (i + 1);
                if (tailCnt > 0) {
                    memmove(&arena->slots[i + 2], &arena->slots[i + 1],
                            tailCnt * sizeof(PfExecSlot));
                }
                arena->slots[i + 1].offset = arena->slots[i].offset + need;
                arena->slots[i + 1].size   = rem;
                arena->slots[i + 1].used   = 0;
                arena->slots[i].size       = need;
                arena->slotCount++;
                pf_coalesce_slots(arena);
            }
            break;
        }
    }
    return (uint32_t*)ptr;
}

void pf_exec_free(PfExecArena* arena, void* ptr)
{
    if (!arena || !arena->base || !ptr)
        return;
    uintptr_t off = (uintptr_t)ptr - (uintptr_t)arena->base;
    if (off >= arena->capacity)
        return;

    for (uint32_t i = 0; i < arena->slotCount; ++i) {
        if (arena->slots[i].offset == (uint32_t)off) {
            arena->slots[i].used = 0;
            pf_coalesce_slots(arena);
            break;
        }
    }
}

namespace android {

static PfExecArena* getExecArena()
{
#ifdef __SYMBIAN32__
    return pf_symbian_get_exec_arena();
#else
    static PfExecArena sArena;
    static bool sInit = false;
    if (!sInit) {
        const size_t kCap = 256 * 1024;
        void* mem = mmap(0, kCap, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem != MAP_FAILED)
            pf_exec_arena_init(&sArena, mem, kCap);
        sInit = true;
    }
    return sArena.base ? &sArena : 0;
#endif
}

CodeCache* ggl_get_code_cache()
{
#ifdef __SYMBIAN32__
    return pf_symbian_get_code_cache();
#else
    static CodeCache sCache(128 * 1024);
    return &sCache;
#endif
}

// ----------------------------------------------------------------------------

Assembly::Assembly(size_t size)
    : mCount(0), mBase(0), mSize(0)
{
    PfExecArena* arena = getExecArena();
    if (arena) {
        mBase = pf_exec_alloc(arena, size);
        if (!mBase) {
            CodeCache* cc = ggl_get_code_cache();
            while (!mBase && cc && cc->evictLru()) {
                mBase = pf_exec_alloc(arena, size);
            }
        }
        if (mBase)
            mSize = size;
    }
}

Assembly::~Assembly()
{
    if (mBase) {
        PfExecArena* arena = getExecArena();
        if (arena)
            pf_exec_free(arena, mBase);
        mBase = 0;
    }
}

void Assembly::incStrong(const void*) const
{
    android_atomic_inc(&mCount);
}

void Assembly::decStrong(const void*) const
{
    if (android_atomic_dec(&mCount) == 1) {
        delete this;
    }
}

ssize_t Assembly::size() const
{
    if (!mBase) return NO_MEMORY;
    return (ssize_t)mSize;
}

uint32_t* Assembly::base() const
{
    return mBase;
}

ssize_t Assembly::resize(size_t newSize)
{
    if (!mBase)
        return NO_MEMORY;
    PfExecArena* arena = getExecArena();
    if (arena)
        mBase = pf_exec_shrink(arena, mBase, newSize);
    mSize = newSize;
    return size();
}

// ----------------------------------------------------------------------------

CodeCache::CodeCache(size_t size)
    : mWhen(0), mCacheSize(size), mCacheInUse(0)
{
}

CodeCache::~CodeCache()
{
}

sp<Assembly> CodeCache::lookup(const AssemblyKeyBase& keyBase) const
{
    Mutex::Autolock _l(mLock);
    sp<Assembly> r;
    ssize_t index = mCacheData.indexOfKey(key_t(keyBase));
    if (index >= 0) {
        const cache_entry_t& e = mCacheData.valueAt(index);
        e.when = mWhen++;
        r = e.entry;
    }
    return r;
}

bool CodeCache::evictLru()
{
    Mutex::Autolock _l(mLock);
    size_t count = mCacheData.size();
    if (count == 0)
        return false;
    size_t lru = 0;
    for (size_t i = 1; i < count; i++) {
        const cache_entry_t& e = mCacheData.valueAt(i);
        if (e.when < mCacheData.valueAt(lru).when) {
            lru = i;
        }
    }
    const cache_entry_t& e = mCacheData.valueAt(lru);
    mCacheInUse -= (size_t)e.entry->size();
    mCacheData.removeItemsAt(lru);
    return true;
}

int CodeCache::cache(const AssemblyKeyBase& keyBase,
                     const sp<Assembly>& assembly)
{
    Mutex::Autolock _l(mLock);

    const ssize_t assemblySize = assembly->size();
    if (assemblySize <= 0 || (size_t)assemblySize > mCacheSize)
        return -1;

    while (mCacheInUse + (size_t)assemblySize > mCacheSize) {
        size_t count = mCacheData.size();
        if (count == 0)
            return -1;
        size_t lru = 0;
        for (size_t i = 1; i < count; i++) {
            const cache_entry_t& e = mCacheData.valueAt(i);
            if (e.when < mCacheData.valueAt(lru).when) {
                lru = i;
            }
        }
        const cache_entry_t& e = mCacheData.valueAt(lru);
        mCacheInUse -= (size_t)e.entry->size();
        mCacheData.removeItemsAt(lru);
    }

    ssize_t err = mCacheData.add(key_t(keyBase), cache_entry_t(assembly, mWhen));
    if (err >= 0) {
        mCacheInUse += (size_t)assemblySize;
        mWhen++;
        void* base = assembly->base();
        uint32_t alignedBytes = pf_align32((size_t)assemblySize);
        void* curr = (uint8_t*)base + alignedBytes;
#ifdef __SYMBIAN32__
        sym_User_IMB_Range(base, curr);
#elif defined(__GNUC__)
        __builtin___clear_cache((char*)base, (char*)curr);
#endif
    }

    return (int)err;
}

}; // namespace android
