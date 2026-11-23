#ifndef PIXELFLINGER_SHIM_CUTILS_LOG_H
#define PIXELFLINGER_SHIM_CUTILS_LOG_H

#ifdef __SYMBIAN32__
#include "symbian_os.h"
#define PF_ABORT() sym_User_Invariant()
#else
#include <stdlib.h>
#define PF_ABORT() abort()
#endif

#ifndef CONDITION
#define CONDITION(cond) (__builtin_expect((cond) != 0, 0))
#endif

#define ALOGV(...)          ((void)0)
#define ALOGD(...)          ((void)0)
#define ALOGI(...)          ((void)0)
#define ALOGW(...)          ((void)0)
#define ALOGE(...)          ((void)0)
#define ALOG(priority, tag, ...) ((void)0)

#define ALOGD_IF(cond, ...) ((void)(cond))
#define ALOGW_IF(cond, ...) ((void)(cond))
#define ALOGE_IF(cond, ...) ((void)(cond))
#define ALOG_ASSERT(cond, ...) ((void)(cond))
#define LOG_ASSERT(cond, ...)  ((void)(cond))

#define LOG_FATAL(...)      do { PF_ABORT(); } while (0)
#define LOG_ALWAYS_FATAL(...) do { PF_ABORT(); } while (0)
#define LOG_FATAL_IF(cond, ...) \
    do { if (CONDITION(cond)) { PF_ABORT(); } } while (0)
#define LOG_ALWAYS_FATAL_IF(cond, ...) \
    do { if (CONDITION(cond)) { PF_ABORT(); } } while (0)

#endif /* PIXELFLINGER_SHIM_CUTILS_LOG_H */
