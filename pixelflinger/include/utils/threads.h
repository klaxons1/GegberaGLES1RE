#ifndef PIXELFLINGER_SHIM_UTILS_THREADS_H
#define PIXELFLINGER_SHIM_UTILS_THREADS_H

namespace android {

class Mutex
{
public:
    inline Mutex() { }
    inline ~Mutex() { }
    inline int lock() { return 0; }
    inline void unlock() { }

    class Autolock
    {
    public:
        inline explicit Autolock(Mutex& mutex) : mLock(mutex) { mLock.lock(); }
        inline explicit Autolock(Mutex* mutex) : mLock(*mutex) { mLock.lock(); }
        inline ~Autolock() { mLock.unlock(); }
    private:
        Mutex& mLock;
    };
};

} // namespace android

#endif /* PIXELFLINGER_SHIM_UTILS_THREADS_H */
