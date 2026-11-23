#ifndef PIXELFLINGER_SHIM_CUTILS_PROPERTIES_H
#define PIXELFLINGER_SHIM_CUTILS_PROPERTIES_H

#include <string.h>

#define PROPERTY_KEY_MAX   32
#define PROPERTY_VALUE_MAX 92

static inline int property_get(const char *key, char *value, const char *default_value)
{
    (void)key;
    if (default_value != NULL) {
        size_t len = strlen(default_value);
        if (len >= PROPERTY_VALUE_MAX)
            len = PROPERTY_VALUE_MAX - 1;
        memcpy(value, default_value, len);
        value[len] = '\0';
        return (int)len;
    }
    value[0] = '\0';
    return 0;
}

#endif /* PIXELFLINGER_SHIM_CUTILS_PROPERTIES_H */
