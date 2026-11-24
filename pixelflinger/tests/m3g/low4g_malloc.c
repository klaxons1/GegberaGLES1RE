/* Process-wide malloc replacement that keeps every heap block below 2 GB.
 *
 * m3gcore (2003-2012) stores object pointers in 32-bit M3Guint handles. The
 * sandbox has no 32-bit runtime, so the host M3G test builds a non-PIE x86-64
 * executable and makes all heap pointers fit in 32 bits instead. This is test
 * infrastructure only; it is never linked into the Symbian DLL.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define ARENA_SIZE  (768u << 20)
#define NUM_CLASSES 40

typedef struct Header { size_t size; size_t cls; } Header;   /* 16 bytes */
typedef struct FreeNode { struct FreeNode* next; } FreeNode;

static uint8_t* arena;
static size_t arenaUsed;
static FreeNode* freeLists[NUM_CLASSES];
static volatile int lockWord;

static void lock(void)   { while (__sync_lock_test_and_set(&lockWord, 1)) { } }
static void unlock(void) { __sync_lock_release(&lockWord); }

static size_t classOf(size_t n)
{
    size_t c = 0, s = 16;
    while (s < n) { s <<= 1; ++c; }
    return c;
}

static void* allocAligned(size_t n, size_t align)
{
    if (align < 16) align = 16;
    if (n > ((size_t)1 << 34)) { errno = ENOMEM; return NULL; }
    size_t need = n + align + 2 * sizeof(Header);
    size_t cls = classOf(need);
    if (cls >= NUM_CLASSES) { errno = ENOMEM; return NULL; }
    size_t blockSize = (size_t)16 << cls;

    lock();
    if (!arena) {
        arena = (uint8_t*)mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT | MAP_NORESERVE, -1, 0);
        if (arena == MAP_FAILED) { arena = NULL; unlock(); errno = ENOMEM; return NULL; }
    }
    uint8_t* block;
    if (freeLists[cls]) {
        block = (uint8_t*)freeLists[cls];
        freeLists[cls] = freeLists[cls]->next;
    } else {
        if (arenaUsed + blockSize + sizeof(Header) > ARENA_SIZE) { unlock(); errno = ENOMEM; return NULL; }
        block = arena + arenaUsed;
        arenaUsed += blockSize + sizeof(Header);
    }
    unlock();

    /* [block: Header(owner)] ... [Header(payload)][payload] */
    uintptr_t p = ((uintptr_t)block + sizeof(Header) + sizeof(Header) + align - 1) & ~(uintptr_t)(align - 1);
    Header* h = (Header*)(p - sizeof(Header));
    h->size = n;
    h->cls = cls | ((size_t)(p - (uintptr_t)block) << 8);
    return (void*)p;
}

static Header* headerOf(void* p) { return (Header*)((uint8_t*)p - sizeof(Header)); }

void* malloc(size_t n) { return allocAligned(n ? n : 1, 16); }

void free(void* p)
{
    if (!p) return;
    Header* h = headerOf(p);
    size_t cls = h->cls & 0xff;
    uint8_t* block = (uint8_t*)p - (h->cls >> 8);
    lock();
    ((FreeNode*)block)->next = freeLists[cls];
    freeLists[cls] = (FreeNode*)block;
    unlock();
}

void* calloc(size_t a, size_t b)
{
    if (b && a > SIZE_MAX / b) { errno = ENOMEM; return NULL; }
    void* p = malloc(a * b);
    if (p) memset(p, 0, a * b);
    return p;
}

void* realloc(void* p, size_t n)
{
    if (!p) return malloc(n);
    if (!n) { free(p); return NULL; }
    size_t old = headerOf(p)->size;
    void* q = malloc(n);
    if (!q) return NULL;
    memcpy(q, p, old < n ? old : n);
    free(p);
    return q;
}

void* memalign(size_t a, size_t n) { return allocAligned(n ? n : 1, a); }
void* aligned_alloc(size_t a, size_t n) { return allocAligned(n ? n : 1, a); }
int posix_memalign(void** out, size_t a, size_t n)
{
    void* p = allocAligned(n ? n : 1, a);
    if (!p) return ENOMEM;
    *out = p;
    return 0;
}
void* valloc(size_t n) { return allocAligned(n ? n : 1, 4096); }
void* pvalloc(size_t n) { return allocAligned((n + 4095) & ~(size_t)4095, 4096); }
size_t malloc_usable_size(void* p) { return p ? headerOf(p)->size : 0; }

uintptr_t low4g_heap_high_water(void) { return (uintptr_t)arena + arenaUsed; }
