/* grSmall.c - small core utilities (start of the port; grows as pieces of
 * the original grSmall.c are brought over). */
#include "gerbera/grDefs.h"
#include <stdint.h>

/* grIsAligned - verbatim (docs/libGLES_CM.dll.c:13072-13075, 3 TU copies) */
int grIsAligned(void *ptr, int alignment)
{
    return ((uintptr_t)(alignment - 1) & (uintptr_t)ptr) == 0;
}
