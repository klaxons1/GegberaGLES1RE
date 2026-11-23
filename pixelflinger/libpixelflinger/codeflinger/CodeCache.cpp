/* libs/pixelflinger/codeflinger/CodeCache.cpp
**
** Symbian OS EKA2 (zero-WSD RChunk::CreateLocalCode + User::IMB_Range)
** and POSIX mmap CodeCache implementation for Pixelflinger / Codeflinger.
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

static inline size_t pf_align16(size_t n)
{
    return (n + 15u) & ~(size_t)15u;
}

void pf_exec_arena_init(PfExecArena* arena, void* mem, size_t totalBytes)
{
    uintptr_t raw = (uintptr_t)mem;
    uintptr_t aligned = (raw + 15u) & ~(uintptr_t)15u;
    size_t adj = (size_t)(aligned - raw);
    if (!mem || totalBytes <= adj + sizeof(PfExecBlock) + 16u) {
        arena->base = 0;
        arena->capacity = 0;
        arena->head = 0;
        return;
    }
    arena->base = (uint8_t*)aligned;
    arena->capacity = (totalBytes - adj) & ~(size_t)15u;
    PfExecBlock* b = (PfExecBlock*)arena->base;
    b->size = arena->capacity - sizeof(PfExecBlock);
    b->used = 0;
    b->pad  = 0x45584543u;
    b->prev = 0;
    b->next = 0;
    arena->head = b;
}

uint32_t* pf_exec_alloc(PfExecArena* arena, size_t reqBytes)
{
    if (!arena || !arena->head)
        return 0;
    size_t need = pf_align16(reqBytes ? reqBytes : 16u);
    for (PfExecBlock* b = arena->head; b != 0; b = b->next) {
        if (!b->used && b->size >= need) {
            if (b->size >= need + sizeof(PfExecBlock) + 32u) {
                PfExecBlock* tail = (PfExecBlock*)((uint8_t*)(b + 1) + need);
                tail->size = b->size - need - sizeof(PfExecBlock);
                tail->used = 0;
                tail->pad  = 0x45584543u;
                tail->prev = b;
                tail->next = b->next;
                if (b->next)
                    b->next->prev = tail;
                b->next = tail;
                b->size = need;
            }
            b->used = 1;
            return (uint32_t*)(b + 1);
        }
    }
    return 0;
}

uint32_t* pf_exec_shrink(PfExecArena* arena, void* ptr, size_t newBytes)
{
    (void)arena;
    if (!ptr)
        return 0;
    PfExecBlock* b = ((PfExecBlock*)ptr) - 1;
    size_t need = pf_align16(newBytes ? newBytes : 16u);
    if (b->size >= need + sizeof(PfExecBlock) + 16u) {
        PfExecBlock* tail = (PfExecBlock*)((uint8_t*)(b + 1) + need);
        tail->size = b->size - need - sizeof(PfExecBlock);
        tail->used = 0;
        tail->pad  = 0x45584543u;
        tail->prev = b;
        tail->next = b->next;
        if (b->next)
            b->next->prev = tail;
        b->next = tail;
        b->size = need;

        /* Coalesce newly freed tail with following free block if present */
        PfExecBlock* nxt = tail->next;
        if (nxt && !nxt->used) {
            tail->size += sizeof(PfExecBlock) + nxt->size;
            tail->next = nxt->next;
            if (tail->next)
                tail->next->prev = tail;
        }
    }
    return (uint32_t*)ptr;
}

void pf_exec_free(PfExecArena* arena, void* ptr)
{
    (void)arena;
    if (!ptr)
        return;
    PfExecBlock* b = ((PfExecBlock*)ptr) - 1;
    b->used = 0;

    if (b->next && !b->next->used) {
        PfExecBlock* nxt = b->next;
        b->size += sizeof(PfExecBlock) + nxt->size;
        b->next = nxt->next;
        if (b->next)
            b->next->prev = b;
    }
    if (b->prev && !b->prev->used) {
        PfExecBlock* prv = b->prev;
        prv->size += sizeof(PfExecBlock) + b->size;
        prv->next = b->next;
        if (prv->next)
            prv->next->prev = prv;
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
    return sArena.head ? &sArena : 0;
#endif
}

CodeCache* ggl_get_code_cache()
{
#ifdef __SYMBIAN32__
    return pf_symbian_get_code_cache();
#else
    static CodeCache sCache(64 * 1024);
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
        void* curr = (uint8_t*)base + assemblySize;
#ifdef __SYMBIAN32__
        sym_User_IMB_Range(base, curr);
#elif defined(__GNUC__)
        __builtin___clear_cache((char*)base, (char*)curr);
#endif
    }

    return (int)err;
}

}; // namespace android
