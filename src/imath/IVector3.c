/* IVector3.c - shared-exponent 3-vectors.
 *
 * The rsqLUTa/rsqLUTm tables are used by the fast inverse-square-root
 * seeds in normsub/length (they live at the start of the .data section
 * next to the core tables in the reference binary).
 */
#include "imath/imApi.h"
#include "hg/hgInt32.h"
#include "gerbera/grUtil.h"

/* Linear inverse-square-root seed coefficients, indexed by the normalized
 * input's high bits and exponent parity (decompile lines 5949/6208). */
const HGint32 rsqLUTm[256] = {
    268435456, 265302000, 262193024, 259108432, 256048128, 253012016, 250000000, 247011984,
    244047872, 241107568, 239646314, 236741542, 233860338, 231002606, 229582512, 226759808,
    223960336, 222569282, 219804478, 217062666, 215700352, 212992848, 210308096, 208974222,
    206323418, 205006464, 202389392, 201089250, 198505694, 197222256, 194672000, 193405158,
    190887986, 189637632, 188392750, 185919354, 184690816, 182250000, 181037698, 179830784,
    177433072, 176242250, 175056768, 172701776, 171532242, 170368000, 169209038, 166906906,
    165763712, 164625750, 163493008, 161243136, 160125982, 159014000, 157907178, 156805504,
    155708966, 153531250, 152450048, 151373934, 150302896, 149236922, 148176000, 147120118,
    146069264, 143982592, 142946750, 141915888, 140889994, 139869056, 138853062, 137842000,
    136835858, 135834624, 134838286, 133846832, 132860250, 131878528, 130901654, 129929616,
    128962402, 128000000, 127042398, 126089584, 125141546, 124198272, 123259750, 123259750,
    122325968, 121396914, 120472576, 119552942, 118638000, 117727738, 116822144, 115921206,
    115921206, 115024912, 114133250, 113246208, 112363774, 111485936, 110612682, 110612682,
    109744000, 108879878, 108020304, 107165266, 107165266, 106314752, 105468750, 104627248,
    103790234, 103790234, 102957696, 102129622, 101306000, 101306000, 100486818, 99672064,
    98861726, 98861726, 98055792, 97254250, 96457088, 96457088, 95664294, 94875856,
    189751712, 186624000, 185073116, 181997172, 180472064, 178955500, 175947908, 174456832,
    172974204, 170034196, 168576768, 167127692, 165686944, 162830336, 161414428, 160006752,
    158607284, 157216000, 154457888, 153091012, 151732224, 150381500, 149038816, 147704148,
    146377472, 145058764, 143748000, 141150208, 139863132, 138583904, 137312500, 136048896,
    134793068, 133544992, 132304644, 131072000, 129847036, 128629728, 127420052, 126217984,
    126217984, 125023500, 123836576, 122657188, 121485312, 120320924, 119164000, 118014516,
    116872448, 115737772, 115737772, 114610464, 113490500, 112377856, 111272508, 110174432,
    109083604, 109083604, 108000000, 106923596, 105854368, 104792292, 104792292, 103737344,
    102689500, 101648736, 101648736, 100615028, 99588352, 98568684, 98568684, 97556000,
    96550276, 95551488, 95551488, 94559612, 93574624, 93574624, 92596500, 91625216,
    91625216, 90660748, 89703072, 88752164, 88752164, 87808000, 86870556, 86870556,
    85939808, 85939808, 85015732, 84098304, 84098304, 83187500, 82283296, 82283296,
    81385668, 81385668, 80494592, 79610044, 79610044, 78732000, 78732000, 77860436,
    76995328, 76995328, 76136652, 76136652, 75284384, 75284384, 74438500, 73598976,
    73598976, 72765788, 72765788, 71938912, 71938912, 71118324, 71118324, 70304000,
    70304000, 69495916, 69495916, 68694048, 68694048, 67898372, 67898372, 67108864
};
const HGint16 rsqLUTa[256] = {
    0x3000, 0x2FD0, 0x2FA0, 0x2F70, 0x2F40, 0x2F10, 0x2EE0, 0x2EB0,
    0x2E80, 0x2E50, 0x2E38, 0x2E08, 0x2DD8, 0x2DA8, 0x2D90, 0x2D60,
    0x2D30, 0x2D18, 0x2CE8, 0x2CB8, 0x2CA0, 0x2C70, 0x2C40, 0x2C28,
    0x2BF8, 0x2BE0, 0x2BB0, 0x2B98, 0x2B68, 0x2B50, 0x2B20, 0x2B08,
    0x2AD8, 0x2AC0, 0x2AA8, 0x2A78, 0x2A60, 0x2A30, 0x2A18, 0x2A00,
    0x29D0, 0x29B8, 0x29A0, 0x2970, 0x2958, 0x2940, 0x2928, 0x28F8,
    0x28E0, 0x28C8, 0x28B0, 0x2880, 0x2868, 0x2850, 0x2838, 0x2820,
    0x2808, 0x27D8, 0x27C0, 0x27A8, 0x2790, 0x2778, 0x2760, 0x2748,
    0x2730, 0x2700, 0x26E8, 0x26D0, 0x26B8, 0x26A0, 0x2688, 0x2670,
    0x2658, 0x2640, 0x2628, 0x2610, 0x25F8, 0x25E0, 0x25C8, 0x25B0,
    0x2598, 0x2580, 0x2568, 0x2550, 0x2538, 0x2520, 0x2508, 0x2508,
    0x24F0, 0x24D8, 0x24C0, 0x24A8, 0x2490, 0x2478, 0x2460, 0x2448,
    0x2448, 0x2430, 0x2418, 0x2400, 0x23E8, 0x23D0, 0x23B8, 0x23B8,
    0x23A0, 0x2388, 0x2370, 0x2358, 0x2358, 0x2340, 0x2328, 0x2310,
    0x22F8, 0x22F8, 0x22E0, 0x22C8, 0x22B0, 0x22B0, 0x2298, 0x2280,
    0x2268, 0x2268, 0x2250, 0x2238, 0x2220, 0x2220, 0x2208, 0x21F0,
    0x21F0, 0x21C0, 0x21A8, 0x2178, 0x2160, 0x2148, 0x2118, 0x2100,
    0x20E8, 0x20B8, 0x20A0, 0x2088, 0x2070, 0x2040, 0x2028, 0x2010,
    0x1FF8, 0x1FE0, 0x1FB0, 0x1F98, 0x1F80, 0x1F68, 0x1F50, 0x1F38,
    0x1F20, 0x1F08, 0x1EF0, 0x1EC0, 0x1EA8, 0x1E90, 0x1E78, 0x1E60,
    0x1E48, 0x1E30, 0x1E18, 0x1E00, 0x1DE8, 0x1DD0, 0x1DB8, 0x1DA0,
    0x1DA0, 0x1D88, 0x1D70, 0x1D58, 0x1D40, 0x1D28, 0x1D10, 0x1CF8,
    0x1CE0, 0x1CC8, 0x1CC8, 0x1CB0, 0x1C98, 0x1C80, 0x1C68, 0x1C50,
    0x1C38, 0x1C38, 0x1C20, 0x1C08, 0x1BF0, 0x1BD8, 0x1BD8, 0x1BC0,
    0x1BA8, 0x1B90, 0x1B90, 0x1B78, 0x1B60, 0x1B48, 0x1B48, 0x1B30,
    0x1B18, 0x1B00, 0x1B00, 0x1AE8, 0x1AD0, 0x1AD0, 0x1AB8, 0x1AA0,
    0x1AA0, 0x1A88, 0x1A70, 0x1A58, 0x1A58, 0x1A40, 0x1A28, 0x1A28,
    0x1A10, 0x1A10, 0x19F8, 0x19E0, 0x19E0, 0x19C8, 0x19B0, 0x19B0,
    0x1998, 0x1998, 0x1980, 0x1968, 0x1968, 0x1950, 0x1950, 0x1938,
    0x1920, 0x1920, 0x1908, 0x1908, 0x18F0, 0x18F0, 0x18D8, 0x18C0,
    0x18C0, 0x18A8, 0x18A8, 0x1890, 0x1890, 0x1878, 0x1878, 0x1860,
    0x1860, 0x1848, 0x1848, 0x1830, 0x1830, 0x1818, 0x1818, 0x1800
};

void IVector3_init001p(_IVector3 *pD)
{
    pD->e = 127;
    pD->m[0] = 0;
    pD->m[1] = 0;
    pD->m[2] = 0x20000000;
}

void IVector3_zero(_IVector3 *pD)
{
    if (pD == NULL)
        imAssert("pD", "imInline.inl", 0x34u);
    pD->e = 0;
    pD->m[0] = 0;
    pD->m[1] = 0;
    pD->m[2] = 0;
}

void IVector3_set(_IVector3 *pD, HGuint32 e, int x, int y, int z)
{
    int i;
    if (pD == NULL)
        imAssert("pD", "imInline.inl", 0x41u);
    if (!((z | y | x) != 0 || e == IM_EXPONENT_ZERO))
        imAssert("(x|y|z) != 0 || e == IM_EXPONENT_ZERO", "imInline.inl", 0x42u);
    pD->e = e;
    pD->m[0] = x;
    pD->m[1] = y;
    pD->m[2] = z;
    if (e > 0xFEu)
    {
        pD->e = 254;
        for (i = 0; i < 3; ++i)
            pD->m[i] = (pD->m[i] >= 0) ? 0x3FFFFFFF : -1073741823;
    }
    if (IVector3_isValid(pD) == 0)
        imAssert("IVector3_isValid(pD)", "imInline.inl", 0x50u);
}

int IVector3_normsub(int *pD, _IVector3 *pA, _IVector3 *pB)
{
    HGuint32 ea, eb, eMax, diff;
    HGint32 x[3], y[3];
    HGint64 dp;
    int clz, lsh;
    int idx, rsq_m, rsq_e, m, e;

    if (!(pD != NULL && pA != NULL && pB != NULL))
        imAssert("pD && pA && pB", "IVector3.c", 0x38u);
    if (IVector3_isValid(pA) == 0)
        imAssert("IVector3_isValid(pA)", "IVector3.c", 0x39u);
    if (IVector3_isValid(pB) == 0)
        imAssert("IVector3_isValid(pB)", "IVector3.c", 0x3Au);

    ea = pA->e;
    eb = pB->e;
    x[0] = pA->m[0]; x[1] = pA->m[1]; x[2] = pA->m[2];
    y[0] = pB->m[0]; y[1] = pB->m[1]; y[2] = pB->m[2];
    eMax = hgMaxu32(ea, eb);
    diff = hgMinu32(hgDiff32(ea, eb), 0x1Fu);
    if (ea >= eb)
    {
        x[0] -= y[0] >> diff;
        x[1] -= y[1] >> diff;
        x[2] -= y[2] >> diff;
    }
    else
    {
        x[0] = (x[0] >> diff) - y[0];
        x[1] = (x[1] >> diff) - y[1];
        x[2] = (x[2] >> diff) - y[2];
    }
    dp = hgMadd64(hgMadd64(hgMul64(x[0], x[0]), x[1], x[1]), x[2], x[2]);
    clz = hgClz64(dp);
    if (clz >= 64)
    {
        pD[0] = 0;
        pD[1] = 0;
        pD[2] = 0;
        return IScalar_zero();
    }
    else
    {
        m = hgLsl64h(dp, clz);              /* dp normalized: bit 63 = 1 */
        lsh = 1 - clz;
        idx = ((m >> 24) & 0x7F) | (((1 - clz) & 1) << 7);
        rsq_m = 4 * rsqLUTm[idx];
        rsq_m = hgMulu64h(m, rsq_m);        /* linear estimate           */
        rsq_e = lsh >> 1;
        if (!(rsq_e >= -31 && rsq_e <= 0))
            imAssert("rsq_e >= -31 && rsq_e <= 0", "IVector3.c", 120);
        rsq_m = (rsqLUTa[idx] << 17) - rsq_m;
        if ((rsq_m & 0x40000000) == 0)
        {
            ++rsq_e;
            rsq_m *= 2;
        }
        if (!(rsq_e >= -31 && rsq_e <= 1))
            imAssert("rsq_e >= -31 && rsq_e <= 1", "IVector3.c", 130);
        if ((unsigned int)rsq_m < 0x40000000u)
            imAssert("rsq_m >= 0x40000000", "IVector3.c", 131);
        if ((unsigned int)rsq_m > 0x7FFFFFFFu)
            imAssert("rsq_m <= 0x7fffffff", "IVector3.c", 132);
        lsh += eMax + 2 - rsq_e;
        m = hgMulu64h(rsq_m, m);            /* multiply in               */
        rsq_e += 8;
        if (!(rsq_e >= -23 && rsq_e <= 9))
            imAssert("rsq_e >= -23 && rsq_e <= 9", "IVector3.c", 139);
        if (rsq_e > 0)
        {
            rsq_m = (unsigned int)rsq_m >> rsq_e;
            rsq_e = 0;
        }
        rsq_e = -rsq_e;
        if (!(rsq_e >= 0 && rsq_e <= 23))
            imAssert("rsq_e >= 0 && rsq_e <= 23", "IVector3.c", 0x95u);
        pD[0] = hgLsl64h_0_31(hgMul64(rsq_m, x[0]), rsq_e);
        pD[1] = hgLsl64h_0_31(hgMul64(rsq_m, x[1]), rsq_e);
        pD[2] = hgLsl64h_0_31(hgMul64(rsq_m, x[2]), rsq_e);
        e = lsh;
        if ((m & 0x40000000) != 0)
        {
            ++e;
            m = (unsigned int)m >> 1;
        }
        if (e > 0)
        {
            if (e > 254)
            {
                e = 254;
                m = 0x3FFFFFFF;
            }
        }
        else
        {
            e = 0;
            m = 0;
        }
        return IScalar_set(e, m);
    }
}

int IVector3_length(_IVector3 *pA)
{
    HGuint32 e;
    HGint64 dp;
    int mh, sh, shExp;
    int idx, rsq_m, rsq_e, m;

    if (pA == NULL)
        imAssert("pA", "IVector3.c", 0xC6u);
    if (IVector3_isValid(pA) == 0)
        imAssert("IVector3_isValid(pA)", "IVector3.c", 0xC7u);
    e = pA->e;
    if (e == 0)
        return IScalar_zero();
    dp = hgMadd64(hgMadd64(hgMul64(pA->m[0], pA->m[0]), pA->m[1], pA->m[1]),
                  pA->m[2], pA->m[2]);
    mh = (int)hgGet64h(dp);
    sh = (91 >> (2 * (mh >> 27))) & 3;
    if (hgInRange32(hgClz32(mh) - 2, 0, 3) == 0)
        imAssert("hgInRange32(hgClz32(m)-2, 0, 3)", "IVector3.c", 0xE4u);
    if (sh != hgClz32(mh) - 2)
        imAssert("hgClz32(m)-2 == sh", "IVector3.c", 0xE5u);
    mh <<= sh;
    shExp = 3 - sh;
    idx = ((mh >> 22) & 0x7F) | (((3 - sh) & 1) << 7);
    rsq_m = (int)(8u * (unsigned int)rsqLUTm[idx]);
    rsq_m = hgMulu64h(rsq_m, mh);
    rsq_e = shExp >> 1;
    rsq_m = (rsqLUTa[idx] << 17) - 2 * rsq_m;
    if ((rsq_m & 0x40000000) == 0)
    {
        ++rsq_e;
        rsq_m *= 2;
    }
    if ((unsigned int)rsq_m < 0x40000000u)
        imAssert("rsq_m >= IUNITSCALAR_TWO", "IVector3.c", 0x104u);
    if ((unsigned int)rsq_m > 0x7FFFFFFFu)
        imAssert("rsq_m <= 0x7fffffff", "IVector3.c", 0x105u);
    shExp += e - rsq_e;
    m = 4 * hgMulu64h(rsq_m, mh);
    if ((m & 0x40000000) != 0)
    {
        ++shExp;
        m >>= 1;
    }
    if (shExp <= 0)
        imAssert("e > (int)IM_EXPONENT_ZERO", "IVector3.c", 0x112u);
    if (shExp > 254)
    {
        shExp = 254;
        m = 0x3FFFFFFF;
    }
    return IScalar_set(shExp, m);
}

int IVector3_clipperDot(int *weights, int *distances)
{
    int products[3];
    int exps[3];
    int maxExp = 0;
    int i;
    int sum, sh, mt, e;

    for (i = 0; i < 3; ++i)
    {
        if (!(weights[i] >= 0 && weights[i] <= IUNITSCALAR_ONE))
            imAssert("weights[0] >= 0 && weights[0] <= IUNITSCALAR_ONE",
                     "IVector3.c", 0x135u + i);
    }
    for (i = 2; i >= 0; --i)
    {
        int e = IScalar_getExponent(distances[i]);
        if (e != 0 && weights[i] != 0)
        {
            int sh = hgClz32(weights[i]) - 1;
            int shExp = 30 - sh;
            unsigned int um = IScalar_getUnsignedMantissa(distances[i]);
            products[i] = hgMul64h(weights[i] << sh, 2 * um);
            e += shExp;
        }
        else
        {
            products[i] = 0;
            e = 0;
        }
        exps[i] = e;
        if (maxExp < e)
            maxExp = e;
    }
    products[0] >>= hgMinu32(maxExp - exps[0], 0x1Fu);
    products[1] >>= hgMinu32(maxExp - exps[1], 0x1Fu);
    products[2] >>= hgMinu32(maxExp - exps[2], 0x1Fu);
    if (F_ZCMPLT(distances[0]))
        products[0] = -products[0];
    if (F_ZCMPLT(distances[1]))
        products[1] = -products[1];
    if (F_ZCMPLT(distances[2]))
        products[2] = -products[2];
    sum = products[0] + products[1] + products[2];
    sh = hgClz32(hgAbs32(sum)) - 1;
    if (sh < 0)
        imAssert("sh >= 0", "IVector3.c", 357);
    e = maxExp - (sh + 27);
    mt = (int)((unsigned int)sum << sh) >> 1;
    if (hgAbs32(mt) >= 0x40000000)
        imAssert("hgAbs32(mt) < 0x40000000", "IVector3.c", 364);
    if (e > 0 && mt != 0)
    {
        if (e > 254)
        {
            e = 254;
            mt = hgCopySign32(0x3FFFFFFF, mt);
        }
    }
    else
    {
        e = 0;
        mt = 0;
    }
    return IScalar_set(e, mt);
}
