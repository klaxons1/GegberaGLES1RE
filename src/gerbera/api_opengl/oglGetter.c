/* oglGetter.c - OpenGL ES glGet* state table and output converters
 * (port of oglGetter.inl; s_defaultValues @ 0x471A74, s_getterTokens @ 0x471D18,
 * s_getterInfos @ 0x471E4C verified verbatim from docs/libGLES_CM.dll). */
#include <string.h>
#include "gerbera/api_opengl/oglContext.h"

static const _oglGetterValues s_defaultValues =
{
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    { 0, 0, 0 }, { 0, 0 }, { 0, 0 }, { 1, 1, 1, 1 },
    0, 0, 7680, 7680, 5888, 2048, 7425, 519, 1029, 0,
    1, 513, 5379, 2305, 519, 7680, 4352, 4352, 4352, 4352,
    4, 4, 8, 4, 5126, 0, 5126, 0, 4, 5126,
    0, 6, 1024, 16, 2, 0, 1, 2, 65536, 65536,
    4352, 33984, 33984, 2, 4, 5126, 0, 0, 4, 0,
    9, 0, 0, 5121, 0, 0, 0, 0, 0, 0,
    0, 0, 5126, 0, 5121, 6408, 0, 0, 0, 10,
    0xFFFFFFFFu, 0xFFFFFFFFu,
    0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
    { 4, 4 }, { 5126, 5126 }, { 0, 0 }, { 0, 0 }, { 2048, 2048 }, { 0, 0 },
    { 1.0f, 64.0f }, { 1.0f, 64.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f },
    { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.2f, 0.2f, 0.2f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
    { 0x8B90, 0x8B91, 0x8B92, 0x8B93, 0x8B94, 0x8B95, 0x8B96, 0x8B97, 0x8B98, 0x8B99 }
};

static const unsigned short s_getterTokens[154] =
{
    0x0B00u, 0x0B02u, 0x0B03u, 0x0B10u, 0x0B11u, 0x0B12u, 0x0B20u, 0x0B21u,
    0x0B22u, 0x0B44u, 0x0B45u, 0x0B46u, 0x0B50u, 0x0B52u, 0x0B53u, 0x0B54u,
    0x0B57u, 0x0B60u, 0x0B62u, 0x0B63u, 0x0B64u, 0x0B65u, 0x0B66u, 0x0B70u,
    0x0B71u, 0x0B72u, 0x0B73u, 0x0B74u, 0x0B90u, 0x0B92u, 0x0B93u, 0x0B94u,
    0x0B95u, 0x0B96u, 0x0B98u, 0x0BA0u, 0x0BA1u, 0x0BA2u, 0x0BC0u, 0x0BC1u,
    0x0BC2u, 0x0BD0u, 0x0BE0u, 0x0BE1u, 0x0BE2u, 0x0BF0u, 0x0BF2u, 0x0C10u,
    0x0C11u, 0x0C22u, 0x0C23u, 0x0C50u, 0x0C51u, 0x0C52u, 0x0C54u, 0x0CF5u,
    0x0D05u, 0x0D31u, 0x0D32u, 0x0D33u, 0x0D36u, 0x0D38u, 0x0D39u, 0x0D3Au,
    0x0D50u, 0x0D52u, 0x0D53u, 0x0D54u, 0x0D55u, 0x0D56u, 0x0D57u, 0x0DE1u,
    0x2A00u, 0x3000u, 0x3001u, 0x3002u, 0x3003u, 0x3004u, 0x3005u, 0x4000u,
    0x4001u, 0x4002u, 0x4003u, 0x4004u, 0x4005u, 0x4006u, 0x4007u, 0x8037u,
    0x8038u, 0x803Au, 0x8069u, 0x8074u, 0x8075u, 0x8076u, 0x8078u, 0x807Au,
    0x807Bu, 0x807Cu, 0x807Eu, 0x807Fu, 0x8081u, 0x8082u, 0x8083u, 0x8088u,
    0x8089u, 0x808Au, 0x809Du, 0x809Eu, 0x809Fu, 0x80A0u, 0x80A8u, 0x80A9u,
    0x80AAu, 0x80ABu, 0x80E8u, 0x80E9u, 0x8126u, 0x8127u, 0x8128u, 0x8129u,
    0x8192u, 0x846Du, 0x846Eu, 0x84E0u, 0x84E1u, 0x84E2u, 0x86A2u, 0x86A3u,
    0x86A4u, 0x86A9u, 0x86AAu, 0x86ABu, 0x86ADu, 0x8840u, 0x8842u, 0x8844u,
    0x8846u, 0x8847u, 0x8848u, 0x8861u, 0x8894u, 0x8895u, 0x8896u, 0x8897u,
    0x8898u, 0x889Au, 0x889Eu, 0x898Au, 0x898Bu, 0x8B9Au, 0x8B9Bu, 0x8B9Cu,
    0x8B9Eu, 0x8B9Fu
};

static const _oglGetterInfo s_getterInfos[154] =
{
    { 588u,  4u, 4u, oglUnitValueToOutput }, /* [  0] 0x0B00 */
    { 496u,  3u, 4u, oglUnitValueToOutput }, /* [  1] 0x0B02 */
    { 604u,  4u, 4u, oglFloatToOutput     }, /* [  2] 0x0B03 */
    {   0u,  1u, 1u, oglBoolToOutput      }, /* [  3] 0x0B10 */
    { 376u,  1u, 4u, oglFloatToOutput     }, /* [  4] 0x0B11 */
    { 468u,  2u, 4u, oglFloatToOutput     }, /* [  5] 0x0B12 */
    {   1u,  1u, 1u, oglBoolToOutput      }, /* [  6] 0x0B20 */
    { 384u,  1u, 4u, oglFloatToOutput     }, /* [  7] 0x0B21 */
    { 460u,  2u, 4u, oglFloatToOutput     }, /* [  8] 0x0B22 */
    {   2u,  1u, 1u, oglBoolToOutput      }, /* [  9] 0x0B44 */
    {  88u,  1u, 4u, oglIntToOutput       }, /* [ 10] 0x0B45 */
    { 108u,  1u, 4u, oglIntToOutput       }, /* [ 11] 0x0B46 */
    {   3u,  1u, 1u, oglBoolToOutput      }, /* [ 12] 0x0B50 */
    {   4u,  1u, 1u, oglBoolToOutput      }, /* [ 13] 0x0B52 */
    { 572u,  4u, 4u, oglUnitValueToOutput }, /* [ 14] 0x0B53 */
    {  80u,  1u, 4u, oglIntToOutput       }, /* [ 15] 0x0B54 */
    {   5u,  1u, 1u, oglBoolToOutput      }, /* [ 16] 0x0B57 */
    {   6u,  1u, 1u, oglBoolToOutput      }, /* [ 17] 0x0B60 */
    { 356u,  1u, 4u, oglFloatToOutput     }, /* [ 18] 0x0B62 */
    { 364u,  1u, 4u, oglFloatToOutput     }, /* [ 19] 0x0B63 */
    { 368u,  1u, 4u, oglFloatToOutput     }, /* [ 20] 0x0B64 */
    {  76u,  1u, 4u, oglIntToOutput       }, /* [ 21] 0x0B65 */
    { 556u,  4u, 4u, oglUnitValueToOutput }, /* [ 22] 0x0B66 */
    { 476u,  2u, 4u, oglUnitValueToOutput }, /* [ 23] 0x0B70 */
    {   7u,  1u, 1u, oglBoolToOutput      }, /* [ 24] 0x0B71 */
    {   8u,  1u, 1u, oglBoolToOutput      }, /* [ 25] 0x0B72 */
    { 392u,  1u, 4u, oglUnitValueToOutput }, /* [ 26] 0x0B73 */
    { 100u,  1u, 4u, oglIntToOutput       }, /* [ 27] 0x0B74 */
    {   9u,  1u, 1u, oglBoolToOutput      }, /* [ 28] 0x0B90 */
    { 112u,  1u, 4u, oglIntToOutput       }, /* [ 29] 0x0B92 */
    { 340u,  1u, 4u, oglUIntToOutput      }, /* [ 30] 0x0B93 */
    { 116u,  1u, 4u, oglIntToOutput       }, /* [ 31] 0x0B94 */
    {  64u,  1u, 4u, oglIntToOutput       }, /* [ 32] 0x0B95 */
    {  68u,  1u, 4u, oglIntToOutput       }, /* [ 33] 0x0B96 */
    { 336u,  1u, 4u, oglUIntToOutput      }, /* [ 34] 0x0B98 */
    {  72u,  1u, 4u, oglIntToOutput       }, /* [ 35] 0x0BA0 */
    {  10u,  1u, 1u, oglBoolToOutput      }, /* [ 36] 0x0BA1 */
    { 508u,  4u, 4u, oglIntToOutput       }, /* [ 37] 0x0BA2 */
    {  11u,  1u, 1u, oglBoolToOutput      }, /* [ 38] 0x0BC0 */
    {  84u,  1u, 4u, oglIntToOutput       }, /* [ 39] 0x0BC1 */
    { 388u,  1u, 4u, oglUnitValueToOutput }, /* [ 40] 0x0BC2 */
    {  12u,  1u, 1u, oglBoolToOutput      }, /* [ 41] 0x0BD0 */
    {  92u,  1u, 4u, oglIntToOutput       }, /* [ 42] 0x0BE0 */
    {  96u,  1u, 4u, oglIntToOutput       }, /* [ 43] 0x0BE1 */
    {  13u,  1u, 1u, oglBoolToOutput      }, /* [ 44] 0x0BE2 */
    { 104u,  1u, 4u, oglIntToOutput       }, /* [ 45] 0x0BF0 */
    {  14u,  1u, 1u, oglBoolToOutput      }, /* [ 46] 0x0BF2 */
    { 524u,  4u, 4u, oglIntToOutput       }, /* [ 47] 0x0C10 */
    {  15u,  1u, 1u, oglBoolToOutput      }, /* [ 48] 0x0C11 */
    { 540u,  4u, 4u, oglUnitValueToOutput }, /* [ 49] 0x0C22 */
    {  52u,  4u, 1u, oglBoolToOutput      }, /* [ 50] 0x0C23 */
    { 120u,  1u, 4u, oglIntToOutput       }, /* [ 51] 0x0C50 */
    { 124u,  1u, 4u, oglIntToOutput       }, /* [ 52] 0x0C51 */
    { 128u,  1u, 4u, oglIntToOutput       }, /* [ 53] 0x0C52 */
    { 132u,  1u, 4u, oglIntToOutput       }, /* [ 54] 0x0C54 */
    { 136u,  1u, 4u, oglIntToOutput       }, /* [ 55] 0x0CF5 */
    { 140u,  1u, 4u, oglIntToOutput       }, /* [ 56] 0x0D05 */
    { 144u,  1u, 4u, oglIntToOutput       }, /* [ 57] 0x0D31 */
    { 180u,  1u, 4u, oglIntToOutput       }, /* [ 58] 0x0D32 */
    { 184u,  1u, 4u, oglIntToOutput       }, /* [ 59] 0x0D33 */
    { 188u,  1u, 4u, oglIntToOutput       }, /* [ 60] 0x0D36 */
    { 192u,  1u, 4u, oglIntToOutput       }, /* [ 61] 0x0D38 */
    { 204u,  1u, 4u, oglIntToOutput       }, /* [ 62] 0x0D39 */
    { 428u,  2u, 4u, oglIntToOutput       }, /* [ 63] 0x0D3A */
    { 248u,  1u, 4u, oglIntToOutput       }, /* [ 64] 0x0D50 */
    { 252u,  1u, 4u, oglIntToOutput       }, /* [ 65] 0x0D52 */
    { 260u,  1u, 4u, oglIntToOutput       }, /* [ 66] 0x0D53 */
    { 276u,  1u, 4u, oglIntToOutput       }, /* [ 67] 0x0D54 */
    { 320u,  1u, 4u, oglIntToOutput       }, /* [ 68] 0x0D55 */
    {  56u,  1u, 4u, oglIntToOutput       }, /* [ 69] 0x0D56 */
    {  60u,  1u, 4u, oglIntToOutput       }, /* [ 70] 0x0D57 */
    {  48u,  1u, 1u, oglBoolToOutput      }, /* [ 71] 0x0DE1 */
    { 360u,  1u, 4u, oglFloatToOutput     }, /* [ 72] 0x2A00 */
    {  16u,  1u, 1u, oglBoolToOutput      }, /* [ 73] 0x3000 */
    {  17u,  1u, 1u, oglBoolToOutput      }, /* [ 74] 0x3001 */
    {  18u,  1u, 1u, oglBoolToOutput      }, /* [ 75] 0x3002 */
    {  19u,  1u, 1u, oglBoolToOutput      }, /* [ 76] 0x3003 */
    {  20u,  1u, 1u, oglBoolToOutput      }, /* [ 77] 0x3004 */
    {  21u,  1u, 1u, oglBoolToOutput      }, /* [ 78] 0x3005 */
    {  22u,  1u, 1u, oglBoolToOutput      }, /* [ 79] 0x4000 */
    {  23u,  1u, 1u, oglBoolToOutput      }, /* [ 80] 0x4001 */
    {  24u,  1u, 1u, oglBoolToOutput      }, /* [ 81] 0x4002 */
    {  25u,  1u, 1u, oglBoolToOutput      }, /* [ 82] 0x4003 */
    {  26u,  1u, 1u, oglBoolToOutput      }, /* [ 83] 0x4004 */
    {  27u,  1u, 1u, oglBoolToOutput      }, /* [ 84] 0x4005 */
    {  28u,  1u, 1u, oglBoolToOutput      }, /* [ 85] 0x4006 */
    {  29u,  1u, 1u, oglBoolToOutput      }, /* [ 86] 0x4007 */
    {  30u,  1u, 1u, oglBoolToOutput      }, /* [ 87] 0x8037 */
    { 380u,  1u, 4u, oglFloatToOutput     }, /* [ 88] 0x8038 */
    {  31u,  1u, 1u, oglBoolToOutput      }, /* [ 89] 0x803A */
    { 436u,  1u, 4u, oglIntToOutput       }, /* [ 90] 0x8069 */
    {  32u,  1u, 1u, oglBoolToOutput      }, /* [ 91] 0x8074 */
    {  33u,  1u, 1u, oglBoolToOutput      }, /* [ 92] 0x8075 */
    {  34u,  1u, 1u, oglBoolToOutput      }, /* [ 93] 0x8076 */
    {  50u,  1u, 1u, oglBoolToOutput      }, /* [ 94] 0x8078 */
    { 148u,  1u, 4u, oglIntToOutput       }, /* [ 95] 0x807A */
    { 152u,  1u, 4u, oglIntToOutput       }, /* [ 96] 0x807B */
    { 156u,  1u, 4u, oglIntToOutput       }, /* [ 97] 0x807C */
    { 160u,  1u, 4u, oglIntToOutput       }, /* [ 98] 0x807E */
    { 164u,  1u, 4u, oglIntToOutput       }, /* [ 99] 0x807F */
    { 168u,  1u, 4u, oglIntToOutput       }, /* [100] 0x8081 */
    { 172u,  1u, 4u, oglIntToOutput       }, /* [101] 0x8082 */
    { 176u,  1u, 4u, oglIntToOutput       }, /* [102] 0x8083 */
    { 396u,  1u, 4u, oglIntToOutput       }, /* [103] 0x8088 */
    { 404u,  1u, 4u, oglIntToOutput       }, /* [104] 0x8089 */
    { 420u,  1u, 4u, oglIntToOutput       }, /* [105] 0x808A */
    {  35u,  1u, 1u, oglBoolToOutput      }, /* [106] 0x809D */
    {  36u,  1u, 1u, oglBoolToOutput      }, /* [107] 0x809E */
    {  37u,  1u, 1u, oglBoolToOutput      }, /* [108] 0x809F */
    {  38u,  1u, 1u, oglBoolToOutput      }, /* [109] 0x80A0 */
    { 196u,  1u, 4u, oglIntToOutput       }, /* [110] 0x80A8 */
    { 200u,  1u, 4u, oglIntToOutput       }, /* [111] 0x80A9 */
    { 372u,  1u, 4u, oglFloatToOutput     }, /* [112] 0x80AA */
    {  39u,  1u, 1u, oglBoolToOutput      }, /* [113] 0x80AB */
    { 208u,  1u, 4u, oglIntToOutput       }, /* [114] 0x80E8 */
    { 212u,  1u, 4u, oglIntToOutput       }, /* [115] 0x80E9 */
    { 344u,  1u, 4u, oglFloatToOutput     }, /* [116] 0x8126 */
    { 348u,  1u, 4u, oglFloatToOutput     }, /* [117] 0x8127 */
    { 352u,  1u, 4u, oglFloatToOutput     }, /* [118] 0x8128 */
    { 484u,  3u, 4u, oglFloatToOutput     }, /* [119] 0x8129 */
    { 216u,  1u, 4u, oglIntToOutput       }, /* [120] 0x8192 */
    { 444u,  2u, 4u, oglFloatToOutput     }, /* [121] 0x846D */
    { 452u,  2u, 4u, oglFloatToOutput     }, /* [122] 0x846E */
    { 220u,  1u, 4u, oglIntToOutput       }, /* [123] 0x84E0 */
    { 224u,  1u, 4u, oglIntToOutput       }, /* [124] 0x84E1 */
    { 228u,  1u, 4u, oglIntToOutput       }, /* [125] 0x84E2 */
    { 332u,  1u, 4u, oglIntToOutput       }, /* [126] 0x86A2 */
    { 636u, 10u, 4u, oglIntToOutput       }, /* [127] 0x86A3 */
    { 232u,  1u, 4u, oglIntToOutput       }, /* [128] 0x86A4 */
    { 236u,  1u, 4u, oglIntToOutput       }, /* [129] 0x86A9 */
    { 240u,  1u, 4u, oglIntToOutput       }, /* [130] 0x86AA */
    { 244u,  1u, 4u, oglIntToOutput       }, /* [131] 0x86AB */
    {  40u,  1u, 1u, oglBoolToOutput      }, /* [132] 0x86AD */
    {  41u,  1u, 1u, oglBoolToOutput      }, /* [133] 0x8840 */
    { 256u,  1u, 4u, oglIntToOutput       }, /* [134] 0x8842 */
    {  42u,  1u, 1u, oglBoolToOutput      }, /* [135] 0x8844 */
    { 264u,  1u, 4u, oglIntToOutput       }, /* [136] 0x8846 */
    { 268u,  1u, 4u, oglIntToOutput       }, /* [137] 0x8847 */
    { 272u,  1u, 4u, oglIntToOutput       }, /* [138] 0x8848 */
    {  43u,  1u, 1u, oglBoolToOutput      }, /* [139] 0x8861 */
    { 280u,  1u, 4u, oglIntToOutput       }, /* [140] 0x8894 */
    { 284u,  1u, 4u, oglIntToOutput       }, /* [141] 0x8895 */
    { 288u,  1u, 4u, oglIntToOutput       }, /* [142] 0x8896 */
    { 292u,  1u, 4u, oglIntToOutput       }, /* [143] 0x8897 */
    { 296u,  1u, 4u, oglIntToOutput       }, /* [144] 0x8898 */
    { 412u,  1u, 4u, oglIntToOutput       }, /* [145] 0x889A */
    { 300u,  1u, 4u, oglIntToOutput       }, /* [146] 0x889E */
    { 304u,  1u, 4u, oglIntToOutput       }, /* [147] 0x898A */
    { 308u,  1u, 4u, oglIntToOutput       }, /* [148] 0x898B */
    { 312u,  1u, 4u, oglIntToOutput       }, /* [149] 0x8B9A */
    { 316u,  1u, 4u, oglIntToOutput       }, /* [150] 0x8B9B */
    {  44u,  1u, 1u, oglBoolToOutput      }, /* [151] 0x8B9C */
    { 324u,  1u, 4u, oglIntToOutput       }, /* [152] 0x8B9E */
    { 328u,  1u, 4u, oglIntToOutput       } /* [153] 0x8B9F */
};

void oglIntToOutput(void *output, const void *input, int n, OGL_OUTPUTTYPE dataType)
{
    const int *inArr = (const int *)input;
    int i = n - 1;

    OGL_ASSERT_AT("oglGetter.inl", 63, "output", output != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 64,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglGetter.inl", 65, "n > 0", n > 0);
    switch (dataType)
    {
    case OGL_OUTPUTTYPE_INT:
    case OGL_OUTPUTTYPE_FIXED:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = inArr[i];
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FLOAT:
    {
        float *outArr = (float *)output;
        do
        {
            outArr[i] = IScalar_stof(IScalar_intToScalar(inArr[i]));
        }
        while (i-- != 0);
        break;
    }
    default:
    {
        unsigned char *outArr = (unsigned char *)output;
        OGL_ASSERT_AT("oglGetter.inl", 93,
                      "dataType == OGL_OUTPUTTYPE_BOOL",
                      dataType == OGL_OUTPUTTYPE_BOOL);
        do
        {
            outArr[i] = (unsigned char)(inArr[i] != 0);
        }
        while (i-- != 0);
        break;
    }
    }
}

void oglUIntToOutput(void *output, const void *input, int n, OGL_OUTPUTTYPE dataType)
{
    const int *inArr = (const int *)input;
    int i = n - 1;

    OGL_ASSERT_AT("oglGetter.inl", 120, "output", output != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 121,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglGetter.inl", 122, "n > 0", n > 0);
    switch (dataType)
    {
    case OGL_OUTPUTTYPE_INT:
    case OGL_OUTPUTTYPE_FIXED:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = inArr[i];
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FLOAT:
    {
        float *outArr = (float *)output;
        do
        {
            int s;
            if (inArr[i] >= 0)
            {
                s = IScalar_intToScalar(inArr[i]);
            }
            else
            {
                int two  = IScalar_intToScalar(2);
                int half = IScalar_intToScalar((int)((unsigned int)inArr[i] >> 1));
                s = IScalar_mul(half, two);
            }
            outArr[i] = IScalar_stof(s);
        }
        while (i-- != 0);
        break;
    }
    default:
    {
        unsigned char *outArr = (unsigned char *)output;
        OGL_ASSERT_AT("oglGetter.inl", 162,
                      "dataType == OGL_OUTPUTTYPE_BOOL",
                      dataType == OGL_OUTPUTTYPE_BOOL);
        do
        {
            outArr[i] = (unsigned char)(inArr[i] != 0);
        }
        while (i-- != 0);
        break;
    }
    }
}

void oglBoolToOutput(void *output, const void *input, int n, OGL_OUTPUTTYPE dataType)
{
    const unsigned char *inArr = (const unsigned char *)input;
    int i = n - 1;

    OGL_ASSERT_AT("oglGetter.inl", 188, "output", output != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 189,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglGetter.inl", 190, "n > 0", n > 0);
    switch (dataType)
    {
    case OGL_OUTPUTTYPE_INT:
    case OGL_OUTPUTTYPE_FIXED:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = (int)inArr[i];
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FLOAT:
    {
        float *outArr = (float *)output;
        do
        {
            outArr[i] = IScalar_stof(IScalar_intToScalar((int)inArr[i]));
        }
        while (i-- != 0);
        break;
    }
    default:
    {
        unsigned char *outArr = (unsigned char *)output;
        OGL_ASSERT_AT("oglGetter.inl", 218,
                      "dataType == OGL_OUTPUTTYPE_BOOL",
                      dataType == OGL_OUTPUTTYPE_BOOL);
        do
        {
            outArr[i] = (unsigned char)(inArr[i] != 0);
        }
        while (i-- != 0);
        break;
    }
    }
}

void oglIScalarToOutput(void *output, const void *input, int n, OGL_OUTPUTTYPE dataType)
{
    const int *inArr = (const int *)input;
    int i = n - 1;

    OGL_ASSERT_AT("oglGetter.inl", 244, "output", output != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 245,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglGetter.inl", 246, "n > 0 && i >= 0", n > 0 && i >= 0);
    switch (dataType)
    {
    case OGL_OUTPUTTYPE_INT:
    {
        int *outArr = (int *)output;
        int minS    = IScalar_intToScalar((int)0x80000000u);
        int maxS    = IScalar_intToScalar(0x7FFFFFFF);
        do
        {
            int s = inArr[i];
            if (IScalar_cmpge(s, maxS) != 0)
                outArr[i] = 0x7FFFFFFF;
            else if (IScalar_cmple(s, minS) != 0)
                outArr[i] = (int)0x80000000u;
            else
                outArr[i] = IScalar_scalarToFixed(s, 0);
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FLOAT:
    {
        float *outArr = (float *)output;
        do
        {
            outArr[i] = IScalar_stof(inArr[i]);
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FIXED:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = IScalar_scalarToFixed(inArr[i], 16);
        }
        while (i-- != 0);
        break;
    }
    default:
    {
        unsigned char *outArr = (unsigned char *)output;
        OGL_ASSERT_AT("oglGetter.inl", 292,
                      "dataType == OGL_OUTPUTTYPE_BOOL",
                      dataType == OGL_OUTPUTTYPE_BOOL);
        do
        {
            outArr[i] = (unsigned char)IScalar_testne(inArr[i]);
        }
        while (i-- != 0);
        break;
    }
    }
}

void oglFloatToOutput(void *output, const void *input, int n, OGL_OUTPUTTYPE dataType)
{
    const float *inArr = (const float *)input;
    int i = n - 1;

    OGL_ASSERT_AT("oglGetter.inl", 318, "output", output != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 319,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglGetter.inl", 320, "n > 0 && i >= 0", n > 0 && i >= 0);
    switch (dataType)
    {
    case OGL_OUTPUTTYPE_INT:
    {
        int *outArr = (int *)output;
        int minS    = IScalar_intToScalar((int)0x80000000u);
        int maxS    = IScalar_intToScalar(0x7FFFFFFF);
        do
        {
            int s = IScalar_ftos(inArr[i]);
            if (IScalar_cmpge(s, maxS) != 0)
                outArr[i] = 0x7FFFFFFF;
            else if (IScalar_cmple(s, minS) != 0)
                outArr[i] = (int)0x80000000u;
            else
                outArr[i] = IScalar_scalarToFixed(s, 0);
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FLOAT:
    {
        const unsigned int *inBits = (const unsigned int *)input;
        unsigned int *outBits      = (unsigned int *)output;
        do
        {
            outBits[i] = inBits[i];
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FIXED:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = IScalar_scalarToFixed(IScalar_ftos(inArr[i]), 16);
        }
        while (i-- != 0);
        break;
    }
    default:
    {
        const unsigned int *inBits = (const unsigned int *)input;
        unsigned char *outArr      = (unsigned char *)output;
        OGL_ASSERT_AT("oglGetter.inl", 366,
                      "dataType == OGL_OUTPUTTYPE_BOOL",
                      dataType == OGL_OUTPUTTYPE_BOOL);
        do
        {
            outArr[i] = (unsigned char)((inBits[i] & 0x7FFFFFFFu) != 0u);
        }
        while (i-- != 0);
        break;
    }
    }
}

void oglUnitValueToOutput(void *output, const void *input, int n, OGL_OUTPUTTYPE dataType)
{
    const float *inArr = (const float *)input;
    int i = n - 1;

    OGL_ASSERT_AT("oglGetter.inl", 401, "output", output != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 402,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    OGL_ASSERT_AT("oglGetter.inl", 403, "n > 0", n > 0);
    switch (dataType)
    {
    case OGL_OUTPUTTYPE_INT:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = IScalar_scalarToFixed(IScalar_ftos(inArr[i]), 31);
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FLOAT:
    {
        const unsigned int *inBits = (const unsigned int *)input;
        unsigned int *outBits      = (unsigned int *)output;
        do
        {
            outBits[i] = inBits[i];
        }
        while (i-- != 0);
        break;
    }
    case OGL_OUTPUTTYPE_FIXED:
    {
        int *outArr = (int *)output;
        do
        {
            outArr[i] = IScalar_scalarToFixed(IScalar_ftos(inArr[i]), 16);
        }
        while (i-- != 0);
        break;
    }
    default:
    {
        unsigned char *outArr = (unsigned char *)output;
        OGL_ASSERT_AT("oglGetter.inl", 446,
                      "dataType == OGL_OUTPUTTYPE_BOOL",
                      dataType == OGL_OUTPUTTYPE_BOOL);
        do
        {
            outArr[i] = (unsigned char)IScalar_testne(IScalar_ftos(inArr[i]));
        }
        while (i-- != 0);
        break;
    }
    }
}

const _oglGetterInfo *oglFindGetterInfo(unsigned int token)
{
    int ndx = oglBinarySearch((int)token, 154, s_getterTokens);
    if (ndx < 0)
        return NULL;
    OGL_ASSERT_AT("oglGetter.inl", 506,
                  "hgInRange32(ndx, 0, HG_LENGTH_OF_ARRAY(s_getterInfos) - 1)",
                  hgInRange32(ndx, 0, 153) != 0);
    return &s_getterInfos[ndx];
}

void oglSetDefaultGetterValues(_oglGetterValues *val)
{
    OGL_ASSERT_AT("oglGetter.inl", 521, "val", val != NULL);
    hgMemCopy(val, &s_defaultValues, (int)sizeof(_oglGetterValues));
}

void oglSetGetterValueBool(oglContext_s *context, int offs, unsigned int pname, const int *val)
{
    const _oglGetterInfo *info;
    unsigned int offset;
    unsigned char *base;
    int i;

    OGL_ASSERT_AT("oglGetter.inl", 542, "context", context != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 543, "val", val != NULL);
    info = oglFindGetterInfo(pname);
    OGL_ASSERT_AT("oglGetter.inl", 550, "info", info != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 551, "info->stride == 1", info->stride == 1u);
    offset = info->offset + info->stride * info->n * (unsigned int)offs;
    OGL_ASSERT_AT("oglGetter.inl", 554,
                  "offset < sizeof(oglGetterValues)",
                  offset < (unsigned int)sizeof(_oglGetterValues));
    base = (unsigned char *)&context->getValues;
    for (i = (int)info->n - 1; i >= 0; --i)
        base[offset + (unsigned int)i] = (unsigned char)val[i];
}

void oglSetGetterValueInt(oglContext_s *context, int offs, unsigned int pname, const int *val)
{
    const _oglGetterInfo *info;
    unsigned int offset;
    unsigned char *base;
    int i;

    OGL_ASSERT_AT("oglGetter.inl", 577, "context", context != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 578, "val", val != NULL);
    info = oglFindGetterInfo(pname);
    OGL_ASSERT_AT("oglGetter.inl", 585, "info", info != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 586, "info->stride == 4", info->stride == 4u);
    offset = info->offset + info->stride * info->n * (unsigned int)offs;
    OGL_ASSERT_AT("oglGetter.inl", 589,
                  "offset < sizeof(oglGetterValues)",
                  offset < (unsigned int)sizeof(_oglGetterValues));
    base = (unsigned char *)&context->getValues;
    for (i = (int)info->n - 1; i >= 0; --i)
        memcpy(base + offset + 4u * (unsigned int)i, &val[i], sizeof(int));
}

void oglSetGetterValueEnum(oglContext_s *context, int offs, unsigned int pname, const unsigned int *val)
{
    oglSetGetterValueInt(context, offs, pname, (const int *)val);
}

void oglSetGetterValueFloat(oglContext_s *context, int offs, unsigned int pname, const float *val)
{
    const _oglGetterInfo *info;
    unsigned int offset;
    unsigned char *base;
    int i;

    OGL_ASSERT_AT("oglGetter.inl", 625, "context", context != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 626, "val", val != NULL);
    info = oglFindGetterInfo(pname);
    OGL_ASSERT_AT("oglGetter.inl", 633, "info", info != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 634, "info->stride == 4", info->stride == 4u);
    offset = info->offset + info->stride * info->n * (unsigned int)offs;
    OGL_ASSERT_AT("oglGetter.inl", 637,
                  "offset < sizeof(oglGetterValues)",
                  offset < (unsigned int)sizeof(_oglGetterValues));
    base = (unsigned char *)&context->getValues;
    for (i = (int)info->n - 1; i >= 0; --i)
    {
        float clean = IScalar_stof(IScalar_ftos(val[i]));
        memcpy(base + offset + 4u * (unsigned int)i, &clean, sizeof(float));
    }
}

void oglGetInternal(oglContext_s *context, unsigned int pname, void *params, OGL_OUTPUTTYPE dataType)
{
    const _oglGetterInfo *info;

    OGL_ASSERT_AT("oglGetter.inl", 647, "context", context != NULL);
    OGL_ASSERT_AT("oglGetter.inl", 648,
                  "hgInRange32(dataType, 0, OGL_OUTPUTTYPE_MAX - 1)",
                  hgInRange32((int)dataType, 0, OGL_OUTPUTTYPE_MAX - 1) != 0);
    if (params == NULL)
    {
        oglSetError(context, OGL_ERROR_INVALID_VALUE);
        return;
    }

    info = oglFindGetterInfo(pname);
    if (hgInRange32((int)pname, GL_MODELVIEW_MATRIX, GL_TEXTURE_MATRIX) != 0)
    {
        int mode = (int)(pname - GL_MODELVIEW_MATRIX);
        OGL_ASSERT_AT("oglGetter.inl", 659, "!info", info == NULL);
        OGL_ASSERT_AT("oglGetter.inl", 662,
                      "mode == GR_MATRIXMODE_MODELVIEW || mode == GR_MATRIXMODE_PROJECTION || mode == GR_MATRIXMODE_TEXTURE0",
                      mode == GR_MATRIXMODE_MODELVIEW || mode == GR_MATRIXMODE_PROJECTION || mode == GR_MATRIXMODE_TEXTURE0);
        if (mode == GR_MATRIXMODE_TEXTURE0)
            mode = (int)context->activeTexture + GR_MATRIXMODE_TEXTURE0;
        oglMatrixToOutput(params, oglGetMatrix(context, mode), dataType);
    }
    else if (hgInRange32((int)pname, GL_MODELVIEW_STACK_DEPTH, GL_TEXTURE_STACK_DEPTH) != 0)
    {
        int mode = (int)(pname - GL_MODELVIEW_STACK_DEPTH);
        oglMatrixStack_s *stack;
        int depthVal;

        OGL_ASSERT_AT("oglGetter.inl", 678, "!info", info == NULL);
        OGL_ASSERT_AT("oglGetter.inl", 681,
                      "mode == GR_MATRIXMODE_MODELVIEW || mode == GR_MATRIXMODE_PROJECTION || mode == GR_MATRIXMODE_TEXTURE0",
                      mode == GR_MATRIXMODE_MODELVIEW || mode == GR_MATRIXMODE_PROJECTION || mode == GR_MATRIXMODE_TEXTURE0);
        if (mode == GR_MATRIXMODE_TEXTURE0)
            mode = (int)context->activeTexture + GR_MATRIXMODE_TEXTURE0;
        stack = oglGetMatrixStack(context, mode);
        OGL_ASSERT_AT("oglGetter.inl", 687, "stack", stack != NULL);
        depthVal = (int)stack->index + 1;
        oglIntToOutput(params, &depthVal, 1, dataType);
    }
    else if (pname == GL_STENCIL_CLEAR_VALUE || pname == GL_STENCIL_REF)
    {
        _grSurfaceDescriptor desc;
        int mask;
        int query;
        int val;

        grGetDrawBufferDescriptor(context->gerbera, &desc);
        mask  = (1 << (8 * (int)desc.stencilBytes)) - 1;
        query = (pname == GL_STENCIL_REF) ? GR_QUERY_STENCIL_REF : GR_QUERY_STENCIL_CLEAR;
        val   = mask & grGetInteger(context->gerbera, query);
        oglIntToOutput(params, &val, 1, dataType);
    }
    else if (info != NULL)
    {
        unsigned int offset = info->offset;
        const unsigned char *base = (const unsigned char *)&context->getValues;

        if (pname == GL_TEXTURE_2D ||
            pname == GL_CURRENT_TEXTURE_COORDS ||
            pname == GL_TEXTURE_STACK_DEPTH ||
            pname == GL_TEXTURE_BINDING_2D)
        {
            offset += info->stride * info->n * (unsigned int)context->activeTexture;
        }
        if (hgInRange32((int)pname, GL_TEXTURE_COORD_ARRAY_SIZE, GL_TEXTURE_COORD_ARRAY_STRIDE) != 0 ||
            pname == GL_TEXTURE_COORD_ARRAY ||
            pname == GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING)
        {
            offset += info->stride * info->n * (unsigned int)context->clientActiveTexture;
        }
        info->outputFunc(params, base + offset, (int)info->n, dataType);
    }
    else
    {
        oglSetError(context, OGL_ERROR_INVALID_ENUM);
    }
}
