/* symbian_os.h - Clean-room Symbian OS v9.x (EKA2 / S60 3rd Ed+) C bindings
 * matching the exact ordinal imports and C++ vtable/struct layouts used by
 * Nokia's reference libgles_cm.dll (UID3 0x101fcabd, version 10.0).
 */
#ifndef SYMBIAN_OS_H
#define SYMBIAN_OS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Basic Symbian geometry types (POD, 8/16 bytes) ----------------------- */

typedef struct SymTPoint
{
    int iX;
    int iY;
} SymTPoint;

typedef struct SymTSize
{
    int iWidth;
    int iHeight;
} SymTSize;

typedef struct SymTRect
{
    SymTPoint iTl;
    SymTPoint iBr;
} SymTRect;

typedef struct SymRArrayBase
{
    int count;
    void *entries;
    int entrySize, keyOffset, allocated, granularity, spare1, spare2;
} SymRArrayBase;

/* --- HAL attribute constants ---------------------------------------------- */
#define SYM_HAL_MEMORY_RAM 15 /* HALData::EMemoryRAM */

/* --- TLS helpers (implemented in symbian_entry.S via UserSvr::Dll*Tls) ---- */
void *sym_DllTls(void);
int   sym_DllSetTls(void *ptr);
void  sym_DllFreeTls(void);

/* --- euser{000a0000}[100039e5].dll ordinal imports ------------------------ */
void  sym_RAllocator_Close(void *alloc);                                    /* @ 32   */
void  sym_RArrayBase_Close(SymRArrayBase *array);                           /* @ 46   */
int   sym_RArrayBase_Append(SymRArrayBase *array, const void *entry);        /* @ 48   */
void  sym_RArrayBase_HeapSort(SymRArrayBase *array,
                             int (*compare)(const void *, const void *)); /* @ 53   */
void  sym_RArrayBase_Ctor(SymRArrayBase *array, int entrySize);              /* @ 59   */
void  sym_RHandleBase_Close(void *handle);                                  /* @ 120  */
void  sym_User_LeaveIfError(int err);                                       /* @ 593  */
void  sym_User_Free(void *ptr);                                             /* @ 644  */
void *sym_User_Alloc(int size);                                             /* @ 646  */
void *sym_User_AllocZ(int size);                                            /* @ 652  */
void *sym_User_AllocZL(int size);                                           /* @ 654  */
void  sym_User_IMB_Range(void *start, void *end);                           /* @ 667  */
void  sym_User_Invariant(void) __attribute__((noreturn));                   /* @ 669  */
int   sym_RChunk_CreateLocalCode(void *chunk, int size, int maxSize,
                                 int ownerType);                            /* @ 905  */
void  sym_UserSvr_DllFreeTls(int handle);                                   /* @ 1210 */
void *sym_UserHeap_ChunkHeap(const void *name, int minLen, int maxLen,
                             int growBy, int align, int singleThread);      /* @ 1363 */
void *sym_RArrayBase_At(const SymRArrayBase *array, int index);              /* @ 1507 */
int   sym_TRect_IsEmpty(const SymTRect *rect);                              /* @ 1652 */
void *sym_RChunk_Base(const void *chunk);                                   /* @ 1702 */
int   sym_RChunk_Size(const void *chunk);                                   /* @ 1703 */
void *sym_memclr(void *trg, unsigned int len);                              /* @ 1951 */
int   sym_memcompare(const unsigned char *left, int leftL,
                     const unsigned char *right, int rightL);               /* @ 1952 */
void *sym_memmove(void *trg, const void *src, unsigned int len);            /* @ 1954 */
void *sym_wordmove(void *trg, const void *src, unsigned int len);           /* @ 1956 */
void *sym_User_MarkCleanupStack(void);                                      /* @ 2060 */
void  sym_User_UnMarkCleanupStack(void *trapHandler);                       /* @ 2061 */
int   sym_XLeaveException_GetReason(const void *exception);                 /* @ 2082 */
void *sym_UserSvr_DllTls(int handle, int dllUid);                           /* @ 2176 */
int   sym_UserSvr_DllSetTls(int handle, int dllUid, void *ptr);             /* @ 2177 */

/* --- ws32{000a0000}[10003b21].dll ordinal imports ------------------------- */
void  sym_RWindowTreeNode_Close(void *win);                                 /* @ 224  */
void  sym_RWindow_Invalidate(void *win);                                    /* @ 264  */
void  sym_RWindow_BeginRedraw(void *win);                                   /* @ 266  */
int   sym_RWindow_Construct(void *win, const void *parent,
                            unsigned int clientHandle);                     /* @ 275  */
void  sym_RWindow_EndRedraw(void *win);                                     /* @ 276  */
void *sym_RWindow_Ctor(void *win, void *wsSession);                         /* @ 280  */
void *sym_CWindowGc_Ctor(void *gc, void *screenDev);                        /* @ 352  */
void  sym_CWsBitmap_Reset(void *wsBmp);                                     /* @ 358  */
int   sym_CWsBitmap_Create(void *wsBmp, const SymTSize *sizeInPixels,
                           int dispMode);                                   /* @ 359  */
void *sym_CWsBitmap_Ctor(void *wsBmp, void *wsSession);                     /* @ 363  */
int   sym_RWsSession_GetDefModeMaxNumColors(const void *wsSession,
                                            int *color, int *gray);         /* @ 384  */
void  sym_RWindowBase_Size(SymTSize *outSize, const void *win);             /* @ 387  */
void  sym_RDrawableWindow_GetDrawRect(SymTRect *outRect, const void *win);  /* @ 490  */

/* --- fbscli{000a0000}[10003a15].dll ordinal imports ----------------------- */
int   sym_CFbsBitmap_ScanLineLength(int width, int dispMode);               /* @ 14   */
void  sym_CFbsBitmap_Reset(void *bmp);                                      /* @ 25   */
int   sym_CFbsBitmap_Resize(void *bmp, const SymTSize *sizeInPixels);       /* @ 27   */
void  sym_CFbsBitmap_UnlockHeap(const void *bmp, int always);               /* @ 107  */
void *sym_CFbsBitmap_DataAddress(const void *bmp);                          /* @ 108  */
int   sym_CFbsBitmap_DisplayMode(const void *bmp);                          /* @ 109  */
void  sym_CFbsBitmap_SizeInPixels(SymTSize *outSize, const void *bmp);      /* @ 117  */
void  sym_CFbsBitmap_LockHeap(const void *bmp, int always);                 /* @ 133  */

/* --- cone{000a0000}[10003a41].dll ordinal imports ------------------------- */
void  sym_CCoeEnv_ConstructL(void *coeEnv);                                 /* @ 136  */
void *sym_CCoeEnv_Static(void);                                             /* @ 182  */
void *sym_CCoeEnv_Ctor(void *coeEnv);                                       /* @ 192  */
/* ARM EHABI TRAP_IGNORE around ConstructL, implemented in symbian_entry.S. */
void  sym_CCoeEnv_ConstructTrapped(void *coeEnv);

/* --- hal{000a0000}[100039e8].dll & gdi{000a0000}[10003b15].dll ------------ */
int   sym_HAL_Get(int attr, int *val);                                      /* @ 1    */
int   sym_TDisplayModeUtils_NumDisplayModeBitsPerPixel(int dispMode);       /* @ 98   */

/* --- Symbian EABI C++ vtable call helpers (verified from epoc binary) ----- */

/* RAllocator / RHeap vtable:
 *   slot 0 (+0):  TAny* Alloc(TInt aSize)
 *   slot 1 (+4):  void  Free(TAny* aPtr)
 *   slot 4 (+16): TInt  Compress() */
static inline void *sym_RHeap_Alloc(void *heap, int size)
{
    void *(* const *vtbl)(void *, int) = *(void *(* const **)(void *, int))heap;
    return vtbl[0](heap, size);
}

static inline void sym_RHeap_Free(void *heap, void *ptr)
{
    void (* const *vtbl)(void *, void *) = *(void (* const **)(void *, void *))heap;
    vtbl[1](heap, ptr);
}

static inline int sym_RHeap_Compress(void *heap)
{
    int (* const *vtbl)(void *) = *(int (* const **)(void *))heap;
    return vtbl[4](heap);
}

/* CBase deleting destructor: slot 1 (+4) in ARM Itanium C++ EABI vtable */
static inline void sym_CBase_Delete(void *obj)
{
    if (obj != NULL)
    {
        void (* const *vtbl)(void *) = *(void (* const **)(void *))obj;
        vtbl[1](obj);
    }
}

/* CCoeEnv layout & vtable:
 *   sizeof(CCoeEnv) = 0x98 (152 bytes)
 *   +44 (0x2C): RWsSession iWsSession
 *   +56 (0x38): RWindowGroup iRootWin
 *   +72 (0x48): CWsScreenDevice* iScreen
 *   vtable +24 (slot 6): void DestroyEnvironment() */
#define SYM_CCOEENV_SIZE             0x98
#define SYM_CCOEENV_WSSESSION(env)   ((void *)((unsigned char *)(env) + 44))
#define SYM_CCOEENV_ROOTWIN(env)     ((void *)((unsigned char *)(env) + 56))
#define SYM_CCOEENV_SCREENDEV(env)   (*(void **)((unsigned char *)(env) + 72))

static inline void sym_CCoeEnv_DestroyEnvironment(void *env)
{
    if (env != NULL)
    {
        void (* const *vtbl)(void *) = *(void (* const **)(void *))env;
        vtbl[6](env);
    }
}

/* CWindowGc layout & vtable:
 *   sizeof(CWindowGc) = 20 bytes, sizeof(CWsBitmap) = 32 bytes, sizeof(RWindow) = 24 bytes
 *   vtable +228 (slot 57): void BitBlt(const TPoint& aPoint, const CFbsBitmap* aBitmap)
 *   vtable +268 (slot 67): TInt Construct()
 *   vtable +272 (slot 68): void Activate(RDrawableWindow& aDevice)
 *   vtable +276 (slot 69): void Deactivate() */
#define SYM_RWINDOW_SIZE    24
#define SYM_CWINDOWGC_SIZE  20
#define SYM_CWSBITMAP_SIZE  32

static inline int sym_CWindowGc_Construct(void *gc)
{
    int (* const *vtbl)(void *) = *(int (* const **)(void *))gc;
    return vtbl[268 / 4](gc);
}

static inline void sym_CWindowGc_Activate(void *gc, void *win)
{
    void (* const *vtbl)(void *, void *) = *(void (* const **)(void *, void *))gc;
    vtbl[272 / 4](gc, win);
}

static inline void sym_CWindowGc_Deactivate(void *gc)
{
    void (* const *vtbl)(void *) = *(void (* const **)(void *))gc;
    vtbl[276 / 4](gc);
}

static inline void sym_CWindowGc_BitBlt(void *gc, const SymTPoint *pt, const void *bmp)
{
    void (* const *vtbl)(void *, const SymTPoint *, const void *) =
        *(void (* const **)(void *, const SymTPoint *, const void *))gc;
    vtbl[228 / 4](gc, pt, bmp);
}

#ifdef __cplusplus
}
#endif

#endif /* SYMBIAN_OS_H */
