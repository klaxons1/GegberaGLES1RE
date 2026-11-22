/* imAssert.c - imath layer assert.
 *
 * Ported from the decompilation (docs/libGLES_CM.dll.c:7629):
 *   void imAssert(expr, file, line)
 *   {
 *       if (grAssert(expr, file, line) != 0)
 *           MEMORY[0] = 0;      // deliberate crash in the reference build
 *   }
 */
#include "imath/imApi.h"
#include "gerbera/grUtil.h"

void imAssert(const char *expr, const char *file, unsigned int line)
{
    if (grAssert(expr, file, (int)line) != 0)
        grDie(0);   /* reference: deliberate null write */
}
