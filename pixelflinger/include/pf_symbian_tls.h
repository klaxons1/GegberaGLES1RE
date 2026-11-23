#ifndef PF_SYMBIAN_TLS_H
#define PF_SYMBIAN_TLS_H

#include <stddef.h>
#include <stdint.h>

namespace android {
class CodeCache;
}

#define PF_EXEC_MAX_SLOTS 256

/* Metadata lives 100% in normal RAM (PfExecArena), NEVER inside executable
 * RChunk pages — avoiding self-modifying-code page invalidations on ARM9/ARM11
 * hardware and EKA2L1's Dynarmic JIT recompiler. */
struct PfExecSlot {
    uint32_t    offset;
    uint32_t    size;
    uint32_t    used;
};

struct PfExecArena {
    uint8_t*    base;
    uint32_t    capacity;
    uint32_t    slotCount;
    PfExecSlot  slots[PF_EXEC_MAX_SLOTS];
};

#ifdef __SYMBIAN32__
PfExecArena*        pf_symbian_get_exec_arena(void);
android::CodeCache* pf_symbian_get_code_cache(void);
#endif

void      pf_exec_arena_init(PfExecArena* arena, void* mem, size_t totalBytes);
uint32_t* pf_exec_alloc(PfExecArena* arena, size_t reqBytes);
uint32_t* pf_exec_shrink(PfExecArena* arena, void* ptr, size_t newBytes);
void      pf_exec_free(PfExecArena* arena, void* ptr);

#endif /* PF_SYMBIAN_TLS_H */
