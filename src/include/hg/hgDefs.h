/* hgDefs.h - base types and macros for the Hybrid Graphics (hg) support library.
 *
 * Ported from the Gerbera GLES1 driver (libGLES_CM.dll) - the original header
 * lived at renderer/libs/hg/hgDefs.h in the source tree "E:\3d_opengl_es_1_1".
 * Types and macro semantics were reconstructed from the decompilation and the
 * assert strings embedded in the binary.
 */
#ifndef HG_DEFS_H
#define HG_DEFS_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* fixed width integer types                                          */
/* ------------------------------------------------------------------ */
typedef int8_t   HGint8;
typedef int16_t  HGint16;
typedef int32_t  HGint32;
typedef int64_t  HGint64;

typedef uint8_t   HGuint8;
typedef uint16_t  HGuint16;
typedef uint32_t  HGuint32;
typedef uint64_t  HGuint64;

typedef float     HGfloat;
typedef double    HGdouble;
typedef size_t    HGuintptr;   /* integer wide enough to hold a pointer */

/* The driver compiles as C.  When the decompiled sources are reused from
 * C++ translation units the extern "C" wrapper keeps linkage sane. */
#ifdef __cplusplus
#define HG_EXTERN_C extern "C"
#define HG_EXTERN_C_BEGIN extern "C" {
#define HG_EXTERN_C_END }
#else
#define HG_EXTERN_C
#define HG_EXTERN_C_BEGIN
#define HG_EXTERN_C_END
#endif

/* ------------------------------------------------------------------ */
/* booleans - the driver uses int-valued bools throughout             */
/* ------------------------------------------------------------------ */
typedef int HGbool;
#define HG_FALSE 0
#define HG_TRUE  1

#define HG_UNUSED(x) ((void)(x))

/* ------------------------------------------------------------------ */
/* inline / restrict                                                  */
/* ------------------------------------------------------------------ */
#ifndef HG_INLINE
#define HG_INLINE static inline
#endif

#ifndef HG_RESTRICT
#define HG_RESTRICT restrict
#endif

/* ------------------------------------------------------------------ */
/* asserts                                                            */
/*                                                                    */
/* The original binary asserts through __assert(file, line, expr)     */
/* (Symbian __ASSERT_DEBUG) for hg code and through                   */
/* grAssert(expr, file, line) / imAssert(expr, file, line) for the    */
/* gerbera core / imath code.  Call sites in the ported sources keep  */
/* the original file name and line number strings so that assert      */
/* traces stay comparable against the reference binary.               */
/* ------------------------------------------------------------------ */
HG_EXTERN_C_BEGIN

void __assert(const char *file, int line, const char *expr);

#ifndef HG_NO_ASSERTS
#define HG_ASSERT_ENABLED 1
#else
#define HG_ASSERT_ENABLED 0
#endif

#if HG_ASSERT_ENABLED
#define HG_ASSERT_AT(file, line, cond)                                       \
    ((cond) ? (void)0 : __assert((file), (line), #cond))
#else
#define HG_ASSERT_AT(file, line, cond) ((void)0)
#endif

HG_EXTERN_C_END

#endif /* HG_DEFS_H */
