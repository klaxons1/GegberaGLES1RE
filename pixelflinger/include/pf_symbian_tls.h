#ifndef PF_SYMBIAN_TLS_H
#define PF_SYMBIAN_TLS_H

#include <stddef.h>
#include <stdint.h>

namespace android {
class CodeCache;
}

struct PfExecBlock {
    size_t          size;   /* Payload bytes (excluding sizeof(PfExecBlock)) */
    uint32_t        used;
    uint32_t        pad;
    PfExecBlock*    prev;
    PfExecBlock*    next;
};

/* sizeof(PfExecBlock) == 16 on 32-bit ARM, 32 on 64-bit host (both 16-byte aligned) */

struct PfExecArena {
    uint8_t*        base;
    size_t          capacity;
    PfExecBlock*    head;
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
