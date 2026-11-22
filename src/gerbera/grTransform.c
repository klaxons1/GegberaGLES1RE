/* grTransform.c - texture-coordinate transform helpers (port of the
 * _helpers_core.c fragment ITCTransform_*; the original TU is unknown -
 * the functions are shared by grSetupCulling, grUpdateVertexPipeline and
 * grTransformTexCoord). The _ITCTransform record layout is documented in
 * grState.h; the fragment has no assert sites. */
#include "gerbera/grState.h"

void ITCTransform_copy(_ITCTransform *pD, _IMatrix4x4 *pM)
{
    int cl = 0;
    int e;
    int ci;
    int ri;
    int ria;

    for (ci = 3; ci >= 0; --ci)
    {
        pD->e[ci] = pM->v[ci].e;
        for (ri = 3; ri >= 0; --ri)
        {
            /* per-row exponent byte minus 119 (the "row 3 at 16.16" bias) */
            e = (int)(unsigned char)(pM->v[ci].e >> (8 * ri)) - 119;
            pD->lsh[ci][ri] = (unsigned char)hgClamp32_0_x(e, 31);
            pD->rsh[ci][ri] = (unsigned char)hgClamp32_0_x(-e, 31);
            pD->m[ci][ri] = pM->v[ci].m[ri];
            if (pD->m[ci][ri] != 0)
            {
                if (pD->m[ci][ri] == 0x20000000 && e == 8)
                    cl |= 2 << (8 * ci + 2 * ri);
                else if (pD->m[ci][ri] == -536870912 && e == 8)
                    cl |= 3 << (8 * ci + 2 * ri);
                else
                    cl |= 1 << (8 * ci + 2 * ri);
            }
        }
    }
    for (ria = 3; ria >= 0; --ria)
        pD->im[ria] = imMulLsl64h(pD->m[3][ria], 0x10000, pD->lsh[3][ria]) >> pD->rsh[3][ria];
    pD->cl = cl;
}
