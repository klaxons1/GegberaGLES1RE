/* hcAdjList.c - compressed interference-graph adjacency list (hcAdjList.c).
 *
 * Compresses an undirected lower-triangular HCAdjMtx_s into delta/reference-
 * encoded lower rows plus run-length-encoded upper spans, and reconstructs
 * the full neighbor list of node `from` in HCAdjList_getEdges().
 *
 * Ported from docs/libGLES_CM.dll.c:42386-42725 (64-bit clean pointer math).
 */
#include "gerbera/hc/hcAdjList.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>

static int HCAdjList_getRowReference(HCAdjList_s *self, int row)
{
    unsigned short *ptr = self->lowerLists[row];

    if ((*ptr & 0x8000u) != 0)
        return *ptr & 0x1FFFu;
    if (ptr[1] != 0)
        return ptr[2];
    return self->numNodes - 1;
}

static int HCAdjList_getRowReferenceLength(HCAdjList_s *self, int row)
{
    unsigned short *ptr = self->lowerLists[row];

    if ((*ptr & 0x8000u) != 0)
        return row;
    return ptr[1];
}

static int HCAdjList_getUpperSize(HCAdjMtx_s *mtx)
{
    int size = 0;
    int nodes = mtx->numNodes;
    int i;

    for (i = 0; i < nodes - 1; ++i)
    {
        int spanVal = HCAdjMtx_hasEdge(mtx, i + 1, i) != 0;
        int j;
        for (j = i + 2; j < nodes; ++j)
        {
            int val = HCAdjMtx_hasEdge(mtx, j, i) != 0;
            if (val != spanVal)
            {
                size += 2;
                spanVal = val;
            }
        }
    }
    return size;
}

static int HCAdjList_getLowerSize(HCAdjMtx_s *mtx)
{
    int nodes = mtx->numNodes;
    int size = nodes + 2;
    int i;

    for (i = 0; i < nodes - 1; ++i)
    {
        if (HCAdjMtx_hasEdge(mtx, nodes - 1, i) != 0)
            ++size;
    }
    for (i = nodes - 2; i >= 0; --i)
    {
        int firstDiff = i;
        int j;
        for (j = 0; j < i; ++j)
        {
            int has0 = HCAdjMtx_hasEdge(mtx, i, j) != 0;
            int has1 = HCAdjMtx_hasEdge(mtx, i + 1, j) != 0;
            if (has0 != has1)
            {
                firstDiff = j;
                break;
            }
        }
        if (firstDiff != i)
        {
            size += 2;
            for (j = firstDiff; j < i; ++j)
            {
                if (HCAdjMtx_hasEdge(mtx, i, j) != 0)
                    ++size;
            }
        }
    }
    return 2 * size;
}

static unsigned short *HCAdjList_compressUpperRow(HCAdjMtx_s *mtx,
                                                  unsigned short *ptr,
                                                  int row,
                                                  unsigned short *flags)
{
    int nodes = mtx->numNodes;

    if (row == nodes - 1)
    {
        *flags |= 0x4000u;
    }
    else
    {
        int spanStart = row + 1;
        int spanVal = HCAdjMtx_hasEdge(mtx, row + 1, row) != 0;
        int j;

        for (j = row + 2; j < nodes; ++j)
        {
            int val = HCAdjMtx_hasEdge(mtx, j, row) != 0;
            if (val != spanVal)
            {
                unsigned short entry = (unsigned short)j;
                if (spanVal != 0)
                    entry |= 0x8000u;
                *ptr++ = entry;
                spanStart = j;
                spanVal = val;
            }
        }
        if (spanStart == row + 1)
        {
            *flags |= 0x4000u;
            if (spanVal != 0)
                *flags |= 0x2000u;
        }
        else
        {
            *(ptr - 1) |= 0x4000u;
        }
    }
    return ptr;
}

static unsigned short *HCAdjList_compressLower(HCAdjList_s *self,
                                               HCAdjMtx_s *mtx,
                                               unsigned short *ptr)
{
    int nodes = mtx->numNodes;
    int lastLen = 0;
    int i;
    unsigned short *cur;

    self->lowerLists[nodes - 1] = ptr;
    ptr[1] = 0;
    ptr[2] = 0;
    cur = ptr + 3;

    for (i = 0; i < nodes - 1; ++i)
    {
        if (HCAdjMtx_hasEdge(mtx, nodes - 1, i) != 0)
        {
            *cur++ = (unsigned short)i;
            ++lastLen;
        }
    }
    *self->lowerLists[nodes - 1] = (unsigned short)lastLen;
    cur = HCAdjList_compressUpperRow(mtx, cur, nodes - 1, self->lowerLists[nodes - 1]);

    for (i = nodes - 2; i >= 0; --i)
    {
        int firstDiff = i;
        int refRow = i + 1;
        int j;
        unsigned short *nextPtr;

        self->lowerLists[i] = cur;
        for (j = 0; j < i; ++j)
        {
            int has0 = HCAdjMtx_hasEdge(mtx, i, j) != 0;
            int has1 = HCAdjMtx_hasEdge(mtx, i + 1, j) != 0;
            if (has0 != has1)
            {
                firstDiff = j;
                break;
            }
        }
        while (HCAdjList_getRowReferenceLength(self, refRow) > firstDiff)
            refRow = HCAdjList_getRowReference(self, refRow);

        if (firstDiff == i)
        {
            *cur = (unsigned short)((unsigned short)refRow + 0x8000u);
            nextPtr = cur + 1;
        }
        else
        {
            int len = 0;
            cur[0] = 0;
            cur[1] = (unsigned short)firstDiff;
            cur[2] = (unsigned short)refRow;
            nextPtr = cur + 3;
            for (j = firstDiff; j < i; ++j)
            {
                if (HCAdjMtx_hasEdge(mtx, i, j) != 0)
                {
                    *nextPtr++ = (unsigned short)j;
                    ++len;
                }
            }
            *self->lowerLists[i] = (unsigned short)len;
        }
        cur = HCAdjList_compressUpperRow(mtx, nextPtr, i, self->lowerLists[i]);
    }
    return cur;
}

HCAdjList_s *HCAdjList_create(HCMemory_s *mgr, HCAdjMtx_s *mtx)
{
    int nNodes;
    HCAdjList_s *self;
    int upperSize;
    int lowerSize;
    unsigned short *mem;
    unsigned short *top;

    if (mgr == NULL)
        HC_assertFail("mgr", "hcAdjList.c", 309, NULL);
    if (mtx == NULL)
        HC_assertFail("mtx", "hcAdjList.c", 310, NULL);

    nNodes = mtx->numNodes;
    if (nNodes <= 0)
        HC_assertFail("nNodes > 0", "hcAdjList.c", 315, NULL);
    if ((nNodes & 0x1FFF) != nNodes)
        HC_assertFail("(nNodes & LOWER_LEN_MASK) == nNodes", "hcAdjList.c", 316, NULL);

    self = (HCAdjList_s *)HCMemory_alloc(
        mgr,
        (unsigned int)offsetof(HCAdjList_s, lowerLists)
            + (unsigned int)nNodes * (unsigned int)sizeof(unsigned short *));
    self->numNodes = nNodes;

    upperSize = HCAdjList_getUpperSize(mtx);
    lowerSize = HCAdjList_getLowerSize(mtx);
    mem = (unsigned short *)HCMemory_alloc(mgr, (unsigned int)(lowerSize + upperSize));
    top = HCAdjList_compressLower(self, mtx, mem);
    if ((unsigned char *)top != (unsigned char *)mem + upperSize + lowerSize)
    {
        HC_assertFail("(HCuint8*)top == (HCuint8*)mem + upperSize + lowerSize",
                      "hcAdjList.c", 331, NULL);
    }
    return self;
}

int HCAdjList_getEdges(HCAdjList_s *self, int from, unsigned short *to)
{
    unsigned short *stack = &to[self->numNodes];
    int stackDepth = 1;
    int count = 0;
    int curRow = from;
    int minRefLen = from;
    int limit;
    unsigned short *upperPtr;
    unsigned short hdr;
    int numNodes;
    int spanStart;

    for (;;)
    {
        int refLen = HCAdjList_getRowReferenceLength(self, curRow);
        if (refLen == 0)
            break;
        if (refLen < minRefLen)
        {
            --stack;
            *stack = (unsigned short)curRow;
            curRow = HCAdjList_getRowReference(self, curRow);
            minRefLen = refLen;
            ++stackDepth;
        }
        else
        {
            curRow = HCAdjList_getRowReference(self, curRow);
        }
    }

    do
    {
        unsigned short *rowPtr;
        int rowLen;

        if (stackDepth-- == 0)
            break;
        limit = from;
        if (stackDepth != 0)
        {
            int nextRefLen = HCAdjList_getRowReferenceLength(self, *stack);
            if (nextRefLen < from)
                limit = nextRefLen;
        }

        rowPtr = self->lowerLists[curRow];
        rowLen = *rowPtr & 0x1FFFu;
        rowPtr += 3;
        while (rowLen-- != 0)
        {
            int col = *rowPtr++;
            if (col >= limit)
                break;
            if (to >= stack)
                HC_assertFail("to < stack", "hcAdjList.c", 457, NULL);
            *to++ = (unsigned short)col;
            ++count;
        }
        if (stackDepth != 0)
            curRow = *stack++;
    }
    while (limit < from);

    upperPtr = self->lowerLists[from];
    hdr = *upperPtr++;
    numNodes = self->numNodes;
    spanStart = from + 1;

    if ((hdr & 0x4000u) != 0)
    {
        if ((hdr & 0x2000u) != 0)
        {
            int k;
            for (k = spanStart; k < numNodes; ++k)
                *to++ = (unsigned short)k;
            return numNodes + count - spanStart;
        }
        return count;
    }
    else
    {
        int spanEnd;
        int spanActive;

        if ((hdr & 0x8000u) == 0)
            upperPtr += (hdr & 0x1FFFu) + 2;

        for (;;)
        {
            unsigned short entry = *upperPtr++;
            spanEnd = entry & 0x3FFFu;
            spanActive = entry & 0x8000u;
            if (spanActive != 0)
            {
                int k;
                for (k = spanStart; k < spanEnd; ++k)
                    *to++ = (unsigned short)k;
                count += spanEnd - spanStart;
            }
            if ((entry & 0x4000u) != 0)
                break;
            spanStart = spanEnd;
        }

        if (spanActive != 0)
        {
            return count;
        }
        else
        {
            int k;
            for (k = spanEnd; k < numNodes; ++k)
                *to++ = (unsigned short)k;
            return numNodes + count - spanEnd;
        }
    }
}
