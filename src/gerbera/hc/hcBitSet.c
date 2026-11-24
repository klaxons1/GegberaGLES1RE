/* hcBitSet.c - arena-allocated dense bitset (hcBitSet.c / hcBitSet.h).
 *
 * Ported from docs/libGLES_CM.dll.c:45506-45798 and inline helpers in
 * hcBitSet.h (HCBitSet_contains, HCBitSet_sub).
 */
#include "gerbera/hc/hcBitSet.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>
#include <string.h>

static unsigned int rol32(unsigned int x, unsigned int s)
{
    s &= 31u;
    if (s == 0)
        return x;
    return (x << s) | (x >> (32u - s));
}

void HCBitSet_selfCheckInternal(HCBitSet_s *self)
{
    (void)self;
}

void HCBitSet_selfCheck(HCBitSet_s *self)
{
    HCBitSet_selfCheckInternal(self);
}

HCBitSet_s *HCBitSet_createNoClear(HCMemory_s *mgr, int length)
{
    HCBitSet_s *result;
    unsigned int nWords;

    if (length <= 0)
        HC_assertFail("length > 0", "hcBitSet.c", 49, NULL);
    nWords = (unsigned int)(length + 31) >> 5;
    result = (HCBitSet_s *)HCMemory_alloc(
        mgr, (unsigned int)offsetof(HCBitSet_s, data) + 4u * nWords);
    result->length = length;
    return result;
}

HCBitSet_s *HCBitSet_create(HCMemory_s *mgr, int length)
{
    HCBitSet_s *self;

    if (length <= 0)
        HC_assertFail("length > 0", "hcBitSet.c", 86, NULL);
    self = HCBitSet_createNoClear(mgr, length);
    HCBitSet_clear(self);
    return self;
}

int HCBitSet_contains(HCBitSet_s *self, int v)
{
    HCBitSet_selfCheck(self);
    if (!(v >= 0 && v < self->length))
        HC_assertFail("(v >= 0) && (v < self->length)", "hcBitSet.h", 100, NULL);
    return (int)(self->data[(unsigned int)v >> 5] & (1u << (v & 0x1F)));
}

void HCBitSet_or(HCBitSet_s *self, int v)
{
    HCBitSet_selfCheck(self);
    if (!(v >= 0 && v < self->length))
        HC_assertFail("v >= 0 && v < self->length", "hcBitSet.c", 102, NULL);
    self->data[(unsigned int)v >> 5] |= (1u << (v & 0x1F));
}

void HCBitSet_sub(HCBitSet_s *self, int v)
{
    HCBitSet_selfCheck(self);
    if (!(v >= 0 && v < self->length))
        HC_assertFail("(v >= 0) && (v < self->length)", "hcBitSet.h", 117, NULL);
    self->data[(unsigned int)v >> 5] &= rol32(0xFFFFFFFEu, (unsigned int)(v & 0x1F));
}

void HCBitSet_copy(HCBitSet_s *dst, HCBitSet_s *src)
{
    if (!(dst != NULL && src != NULL))
        HC_assertFail("dst && src", "hcBitSet.c", 118, NULL);
    HCBitSet_selfCheck(dst);
    HCBitSet_selfCheck(src);
    if (dst->length != src->length)
        HC_assertFail("dst->length == src->length", "hcBitSet.c", 121, NULL);
    memcpy(dst->data, src->data, 4u * ((unsigned int)(src->length + 31) >> 5));
    HCBitSet_selfCheck(dst);
    HCBitSet_selfCheck(src);
}

void HCBitSet_clear(HCBitSet_s *self)
{
    HCBitSet_selfCheck(self);
    memset(self->data, 0, 4u * ((unsigned int)(self->length + 31) >> 5));
}

void HCBitSet_set(HCBitSet_s *self)
{
    unsigned int nDWords;
    int i;

    HCBitSet_selfCheck(self);
    nDWords = (unsigned int)(self->length + 31) >> 5;
    for (i = 0; i < (int)(nDWords - 1u); ++i)
        self->data[i] = 0xFFFFFFFFu;
    self->data[nDWords - 1u] = 0xFFFFFFFFu >> ((32 - (self->length & 0x1F)) & 0x1F);
}

int HCBitSet_equal(HCBitSet_s *a, HCBitSet_s *b)
{
    unsigned int nWords;
    unsigned int i;

    if (!(a != NULL && b != NULL))
        HC_assertFail("a && b", "hcBitSet.c", 170, NULL);
    HCBitSet_selfCheck(a);
    HCBitSet_selfCheck(b);
    if (a->length != b->length)
        HC_assertFail("a->length == b->length", "hcBitSet.c", 173, NULL);
    if (a->length == 0)
        HC_assertFail("a->length", "hcBitSet.c", 174, NULL);

    nWords = (unsigned int)(a->length + 31) >> 5;
    for (i = 0; i < nWords; ++i)
    {
        if (a->data[i] != b->data[i])
            return 0;
    }
    return 1;
}

int HCBitSet_numElements(HCBitSet_s *self)
{
    int cnt = 0;
    int elem;

    if (self == NULL)
        HC_assertFail("self", "hcBitSet.c", 194, NULL);
    HCBitSet_selfCheck(self);
    for (elem = HCBitSet_getFirst(self); elem != -1; elem = HCBitSet_getNext(self, elem))
        ++cnt;
    return cnt;
}

int HCBitSet_getFirst(HCBitSet_s *self)
{
    int nWords;
    int i;

    HCBitSet_selfCheck(self);
    nWords = (int)((unsigned int)(self->length + 31) >> 5);
    for (i = 0; i < nWords; ++i)
    {
        unsigned int subS = self->data[i];
        if (subS != 0)
            return 32 * i + HC_bsf(subS);
    }
    return -1;
}

int HCBitSet_getNext(HCBitSet_s *self, int first)
{
    int ndx;
    int ofs;
    int nWords;

    if (self == NULL)
        HC_assertFail("self", "hcBitSet.c", 238, NULL);
    HCBitSet_selfCheck(self);
    if ((unsigned int)first > (unsigned int)self->length)
        HC_assertFail("HC_IN_RANGE(first, 0, self->length)", "hcBitSet.c", 241, NULL);

    ndx = (int)((unsigned int)(first + 1) >> 5);
    ofs = (first + 1) & 0x1F;
    nWords = (int)((unsigned int)(self->length + 31) >> 5);
    while (ndx < nWords)
    {
        unsigned int w = self->data[ndx] >> ofs;
        if (w != 0)
            return ofs + 32 * ndx + HC_bsf(w);
        ++ndx;
        ofs = 0;
    }
    return -1;
}

int HCBitSet_getFirstUnset(HCBitSet_s *self)
{
    int len;
    int nWords;
    int i;
    unsigned int subS = 0;
    int ret;

    HCBitSet_selfCheck(self);
    len = self->length;
    nWords = (int)((unsigned int)(len + 31) >> 5);
    for (i = 0; ; ++i)
    {
        if (i >= nWords)
            return -1;
        subS = self->data[i];
        if (subS != 0xFFFFFFFFu)
            break;
    }
    ret = 32 * i + HC_bsf(~subS);
    return (ret >= len) ? -1 : ret;
}

int HCBitSet_getNextUnset(HCBitSet_s *self, int first)
{
    int len;
    int nWords;
    int ndx;
    int ofs;
    unsigned int w = 0;
    int ret;

    if (self == NULL)
        HC_assertFail("self", "hcBitSet.c", 299, NULL);
    HCBitSet_selfCheck(self);
    if ((unsigned int)first > (unsigned int)self->length)
        HC_assertFail("HC_IN_RANGE(first, 0, self->length)", "hcBitSet.c", 301, NULL);

    len = self->length;
    nWords = (int)((unsigned int)(len + 31) >> 5);
    ndx = (int)((unsigned int)(first + 1) >> 5);
    /* The reference uses a logical shift (x86 0x451688). Its zero-filled
     * high bits can hide unset bits in later words; preserve that behavior.
     * minColor uses a single-word bitset, where this cannot occur. */
    for (ofs = (first + 1) & 0x1F; ; ofs = 0)
    {
        if (ndx >= nWords)
            return -1;
        w = self->data[ndx] >> ofs;
        if (w != 0xFFFFFFFFu)
            break;
        ++ndx;
    }
    ret = ofs + 32 * ndx + HC_bsf(~w);
    return (ret >= len) ? -1 : ret;
}

void HCBitSet_union(HCBitSet_s *dst, HCBitSet_s *src)
{
    unsigned int len;
    unsigned int i;

    if (!(dst != NULL && src != NULL))
        HC_assertFail("dst && src", "hcBitSet.c", 333, NULL);
    HCBitSet_selfCheck(dst);
    HCBitSet_selfCheck(src);
    if (dst->length != src->length)
        HC_assertFail("dst->length == src->length", "hcBitSet.c", 336, NULL);
    if (dst->length == 0)
        HC_assertFail("dst->length", "hcBitSet.c", 337, NULL);

    len = (unsigned int)(dst->length + 31) >> 5;
    for (i = 0; i < len; ++i)
        dst->data[i] |= src->data[i];
}

void HCBitSet_intersect(HCBitSet_s *dst, HCBitSet_s *src)
{
    unsigned int len;
    unsigned int i;

    if (!(dst != NULL && src != NULL))
        HC_assertFail("dst && src", "hcBitSet.c", 355, NULL);
    HCBitSet_selfCheck(dst);
    HCBitSet_selfCheck(src);
    if (dst->length != src->length)
        HC_assertFail("dst->length == src->length", "hcBitSet.c", 358, NULL);

    len = (unsigned int)(dst->length + 31) >> 5;
    for (i = 0; i < len; ++i)
        dst->data[i] &= src->data[i];
}

void HCBitSet_difference(HCBitSet_s *dst, HCBitSet_s *src)
{
    unsigned int len;
    unsigned int i;

    if (!(dst != NULL && src != NULL))
        HC_assertFail("dst && src", "hcBitSet.c", 376, NULL);
    HCBitSet_selfCheck(dst);
    HCBitSet_selfCheck(src);
    if (dst->length != src->length)
        HC_assertFail("dst->length == src->length", "hcBitSet.c", 379, NULL);

    len = (unsigned int)(dst->length + 31) >> 5;
    for (i = 0; i < len; ++i)
        dst->data[i] &= ~src->data[i];
}
