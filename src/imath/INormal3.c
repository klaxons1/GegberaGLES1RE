/* INormal3.c - normals in 10.21 fixed point (three plain ints).
 *
 * Clean port of the decompiled INormal3.c plus the XNormal3 inline
 * functions from imInline.inl / imApi.inl.
 */
#include "imath/imApi.h"
#include "hg/hgInt32.h"
#include "gerbera/grUtil.h"

/* the rsqrt seed tables live in IVector3.c (shared data, one definition) */
extern HGint32 rsqLUTm[256];
extern HGint16 rsqLUTa[256];

void XNormal3_init001n(int *pD)
{
    pD[0] = 0;
    pD[1] = 0;
    pD[2] = -2097152;
}

void XNormal3_init001p(int *pD)
{
    pD[0] = 0;
    pD[1] = 0;
    pD[2] = 0x200000;
}

void XNormal3_setf(int *normal, HGfloat x, HGfloat y, HGfloat z)
{
    normal[0] = IScalar_scalarToFixed(IScalar_ftos(x), 21);
    normal[1] = IScalar_scalarToFixed(IScalar_ftos(y), 21);
    normal[2] = IScalar_scalarToFixed(IScalar_ftos(z), 21);
}

void XNormal3_setb(int *pD, HGint8 vx, HGint8 vy, HGint8 vz)
{
    pD[0] = ((65793 * vx) >> 2) + 8224;
    pD[1] = ((65793 * vy) >> 2) + 8224;
    pD[2] = ((65793 * vz) >> 2) + 8224;
}

void XNormal3_sets(int *pD, HGint16 vx, HGint16 vy, HGint16 vz)
{
    pD[0] = vx << 6;
    pD[1] = vy << 6;
    pD[2] = vz << 6;
}

void XNormal3_setx(int *pD, int vx, int vy, int vz)
{
    pD[0] = imLsl32s(vx, 5);
    pD[1] = imLsl32s(vy, 5);
    pD[2] = imLsl32s(vz, 5);
}

void XNormal3_copy(int *pD, int *pA)
{
    if (!(pD != NULL && pA != NULL))
        imAssert("pD && pA", "imApi.inl", 0x9D8u);
    pD[0] = pA[0];
    pD[1] = pA[1];
    pD[2] = pA[2];
}

void XNormal3_negate(int *pD, int *pA)
{
    if (!(pD != NULL && pA != NULL))
        imAssert("pD && pA", "imApi.inl", 0x608u);
    pD[0] = -pA[0];
    pD[1] = -pA[1];
    pD[2] = -pA[2];
}

int XNormal3_dot(int *pA, int *pB)
{
    HGint64 x64;
    if (!(pA != NULL && pB != NULL))
        imAssert("pA && pB", "INormal3.c", 0x3Au);
    x64 = hgMul64(pA[0], pB[0]);
    x64 = hgMadd64(x64, pA[1], pB[1]);
    x64 = hgMadd64(x64, pA[2], pB[2]);
    return hgLsl64h_0_31(x64, 11);
}

void XNormal3_normalize(int *pD, int *pA)
{
    int nx = pA[0], ny = pA[1], nz = pA[2];
    HGint64 dp;
    int clz, m, idx, am, e, x;

    if (!(pD != NULL && pA != NULL))
        imAssert("pD && pA", "INormal3.c", 0x48u);
    dp = hgMadd64(hgMadd64(hgMul64(nx, nx), ny, ny), nz, nz);
    if (hgIsZero64(dp) != 0)
    {
        if (nx != 0)
            imAssert("nx == 0", "INormal3.c", 133);
        if (ny != 0)
            imAssert("ny == 0", "INormal3.c", 134);
        if (nz != 0)
            imAssert("nz == 0", "INormal3.c", 135);
        /* zero length: results stay zero */
        nx = ny = nz = 0;
    }
    else
    {
        clz = hgClz64(dp);
        m = hgLsl64h(dp, clz);
        idx = ((m >> 24) & 0x7F);
        --clz;
        idx |= (clz & 1) << 7;
        am = 2 * rsqLUTm[idx];
        am = hgMulu64h(m, am);
        e = (clz - 1) >> 1;
        am = (rsqLUTa[idx] << 16) - am;
        if (e < 0)
        {
            e = -e;
            nx >>= e;
            ny >>= e;
            nz >>= e;
        }
        else
        {
            nx <<= e;
            ny <<= e;
            nz <<= e;
        }
        if (am < 0)
            imAssert("(int)am >= 0", "INormal3.c", 0x72u);
        nx = hgMul64h(nx, am) >> 6;
        ny = hgMul64h(ny, am) >> 6;
        nz = hgMul64h(nz, am) >> 6;
        if ((pA[0] & 0x80000000) != (nx & 0x80000000))
            imAssert("(pA[0]&0x80000000) == (nx&0x80000000)", "INormal3.c", 0x78u);
        if ((pA[1] & 0x80000000) != (ny & 0x80000000))
            imAssert("(pA[1]&0x80000000) == (ny&0x80000000)", "INormal3.c", 0x79u);
        if ((pA[2] & 0x80000000) != (nz & 0x80000000))
            imAssert("(pA[2]&0x80000000) == (nz&0x80000000)", "INormal3.c", 0x7Au);
        x = hgMadd64h(hgMadd64(hgMul64(nx, nx), ny, ny), nz, nz);
        if (x != 0 && hgInRange32(x, 1020, 1028) == 0)
            imAssert("x == 0 || hgInRange32(x, 0x400-0x4,0x400+0x4)", "INormal3.c", 0x7Fu);
        pD[0] = nx;
        pD[1] = ny;
        pD[2] = nz;
        return;
    }
    pD[0] = nx;
    pD[1] = ny;
    pD[2] = nz;
}

void XNormal3_accfv(int *pD, int (*pA)[3], int *pS, int n)
{
    HGint64 ax = 0, ay = 0, az = 0;
    int maxExp = 0;
    int i;

    if (!(pD != NULL && pA != NULL && pS != NULL))
        imAssert("pD && pA && pS", "imApi.inl", 1034);
    if (!(n >= 1 && n <= 4))
        imAssert("n >= 1 && n <= 4", "imApi.inl", 1035);
    for (i = 0; i < n; ++i)
    {
        if (IScalar_isValid(pS[i]) == 0)
            imAssert("IScalar_isValid(pS[i])", "imApi.inl", 1048);
        maxExp = (int)hgMaxu32(maxExp, IScalar_getExponent(pS[i]));
    }
    for (i = 0; i < n; ++i)
    {
        int e = IScalar_getExponent(pS[i]);
        if (e != 0)
        {
            int sh = hgMinu32(maxExp - e, 0x1Fu);
            int m = IScalar_getMantissa(pS[i]) >> sh;
            ax = hgMadd64(ax, pA[i][0], m);
            ay = hgMadd64(ay, pA[i][1], m);
            az = hgMadd64(az, pA[i][2], m);
        }
    }
    pD[0] = (int)hgGet64h(ax);
    pD[1] = (int)hgGet64h(ay);
    pD[2] = (int)hgGet64h(az);
}
