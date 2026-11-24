/* Normal transforms with optional renormalization. */
#include "imath/imApi.h"
#include "hg/hgInt32.h"
#include "gerbera/grUtil.h"

void XNormalTransform_mulNormal(int *pD, _XNormalTransform *pM, int *pV, int renormalize)
{
    int vx = pV[0], vy = pV[1], vz = pV[2];
    int s;

    if (!(pD != NULL && pM != NULL && pV != NULL))
        imAssert("pD && pM && pV", "IMatrix3.c", 0x4Bu);
    pD[0] = hgMadd64h(hgMadd64(hgMul64(pM->m[0][0], vx), pM->m[1][0], vy), pM->m[2][0], vz);
    pD[1] = hgMadd64h(hgMadd64(hgMul64(pM->m[0][1], vx), pM->m[1][1], vy), pM->m[2][1], vz);
    pD[2] = hgMadd64h(hgMadd64(hgMul64(pM->m[0][2], vx), pM->m[1][2], vy), pM->m[2][2], vz);
    if (renormalize != 0)
    {
        XNormal3_normalize(pD, pD);
    }
    else
    {
        s = pM->scale;
        if (s <= 0)
        {
            if (s < 0)
            {
                s = -s;
                pD[0] >>= s;
                pD[1] >>= s;
                pD[2] >>= s;
            }
        }
        else
        {
            pD[0] = imLsl32s(pD[0], s);
            pD[1] = imLsl32s(pD[1], s);
            pD[2] = imLsl32s(pD[2], s);
        }
    }
}
