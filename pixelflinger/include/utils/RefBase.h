#ifndef PIXELFLINGER_SHIM_UTILS_REFBASE_H
#define PIXELFLINGER_SHIM_UTILS_REFBASE_H

#include <cutils/atomic.h>
#include "../libpixelflinger/codeflinger/tinyutils/smartpointer.h"

namespace android {

using tinyutils::sp;

template <class T>
class LightRefBase
{
public:
    inline LightRefBase() : mCount(0) { }
    inline void incStrong(const void* /*id*/) const {
        android_atomic_inc(&mCount);
    }
    inline void decStrong(const void* /*id*/) const {
        if (android_atomic_dec(&mCount) == 1) {
            delete static_cast<const T*>(this);
        }
    }
    inline int32_t getStrongCount() const {
        return mCount;
    }
    typedef LightRefBase<T> basetype;

protected:
    inline ~LightRefBase() { }

private:
    mutable volatile int32_t mCount;
};

} // namespace android

#endif /* PIXELFLINGER_SHIM_UTILS_REFBASE_H */
