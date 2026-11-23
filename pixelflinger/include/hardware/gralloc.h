#ifndef PIXELFLINGER_SHIM_HARDWARE_GRALLOC_H
#define PIXELFLINGER_SHIM_HARDWARE_GRALLOC_H

#include <stdint.h>

#define GRALLOC_HARDWARE_MODULE_ID "gralloc"
#define GRALLOC_USAGE_SW_READ_OFTEN 0x00000003

typedef const void *buffer_handle_t;

typedef struct hw_module_t {
    uint32_t tag;
} hw_module_t;

typedef struct gralloc_module_t {
    hw_module_t common;
    int (*lock)(struct gralloc_module_t const* module,
                buffer_handle_t handle, int usage,
                int l, int t, int w, int h, void** vaddr);
    int (*unlock)(struct gralloc_module_t const* module,
                  buffer_handle_t handle);
} gralloc_module_t;

static inline int hw_get_module(const char * /*id*/, const struct hw_module_t **module)
{
    if (module)
        *module = 0;
    return -1;
}

typedef struct ANativeWindowBuffer {
    int width;
    int height;
    int stride;
    int format;
    int usage;
    buffer_handle_t handle;
    inline void incStrong(const void *) const {}
    inline void decStrong(const void *) const {}
} ANativeWindowBuffer;

#endif /* PIXELFLINGER_SHIM_HARDWARE_GRALLOC_H */
