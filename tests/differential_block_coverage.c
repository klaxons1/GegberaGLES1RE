/* Block coverage without a Python callback for every executed block.
 * Unicorn entrypoints and the first-visit classifier are supplied by the
 * driver; this file has no Unicorn header or link-time dependency.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef int (*RegisterRead)(void *, int, void *);
typedef int (*EmulationStop)(void *);
typedef int (*ClassifyBlock)(uint64_t, uint32_t, unsigned int, uint64_t *);

typedef struct Block
{
    uint64_t address;
    uint64_t branch;
    uint32_t size;
    unsigned char occupied;
    unsigned char thumb;
    unsigned char kind;             /* 0: none, 1: conditional, 2: indirect */
} Block;

typedef struct Edge
{
    uint64_t from;
    uint64_t to;
    unsigned char occupied;
} Edge;

typedef struct BlockCoverage
{
    uint64_t begin, end;
    RegisterRead read_register;
    EmulationStop stop;
    ClassifyBlock classify;
    int cpsr_register;              /* -1 for x86 */
    int error;
    int detail;
    size_t block_capacity, edge_capacity;
    size_t block_count, edge_count[2];
    Block *blocks;
    Edge *edges[2];
    uint64_t previous_branch;
    unsigned int previous_kind;
} BlockCoverage;

enum
{
    BLOCK_BAD_RANGE = 1,
    BLOCK_REGISTER_ERROR = 2,
    BLOCK_CLASSIFIER_ERROR = 3,
    BLOCK_CAPACITY_ERROR = 4,
    EDGE_CAPACITY_ERROR = 5
};

static uint64_t mix(uint64_t value)
{
    value ^= value >> 30;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27;
    value *= UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31);
}

static int valid_capacity(size_t capacity, size_t entry_size)
{
    return capacity >= 4 && !(capacity & (capacity - 1)) && capacity <= SIZE_MAX / entry_size;
}

void differential_block_coverage_destroy(void *opaque)
{
    BlockCoverage *state = (BlockCoverage *)opaque;
    if (state)
    {
        free(state->blocks);
        free(state->edges[0]);
        free(state->edges[1]);
        free(state);
    }
}

void *differential_block_coverage_create(uint64_t begin, uint64_t end,
                                       RegisterRead read_register, int cpsr_register,
                                       EmulationStop stop, ClassifyBlock classify,
                                       size_t block_capacity, size_t edge_capacity)
{
    BlockCoverage *state;
    if (begin >= end || !stop || !classify || (cpsr_register >= 0 && !read_register) ||
        !valid_capacity(block_capacity, sizeof(Block)) || !valid_capacity(edge_capacity, sizeof(Edge)))
        return NULL;
    state = (BlockCoverage *)calloc(1, sizeof(*state));
    if (!state)
        return NULL;
    state->blocks = (Block *)calloc(block_capacity, sizeof(Block));
    state->edges[0] = (Edge *)calloc(edge_capacity, sizeof(Edge));
    state->edges[1] = (Edge *)calloc(edge_capacity, sizeof(Edge));
    if (!state->blocks || !state->edges[0] || !state->edges[1])
    {
        differential_block_coverage_destroy(state);
        return NULL;
    }
    state->begin = begin;
    state->end = end;
    state->read_register = read_register;
    state->cpsr_register = cpsr_register;
    state->stop = stop;
    state->classify = classify;
    state->block_capacity = block_capacity;
    state->edge_capacity = edge_capacity;
    return state;
}

static void fail(BlockCoverage *state, void *engine, int error, int detail)
{
    if (!state->error)
    {
        state->error = error;
        state->detail = detail;
    }
    state->stop(engine);
}

static int insert_edge(BlockCoverage *state, unsigned int kind, uint64_t from, uint64_t to)
{
    size_t index = (size_t)mix(from ^ mix(to)) & (state->edge_capacity - 1);
    Edge *entry;
    for (;;)
    {
        entry = &state->edges[kind - 1][index];
        if (!entry->occupied)
        {
            /* Keep probing bounded even when a fixture discovers more edges
             * than expected. Existing entries remain queryable at the limit. */
            if (state->edge_count[kind - 1] >= state->edge_capacity - state->edge_capacity / 4)
                return 0;
            entry->from = from;
            entry->to = to;
            entry->occupied = 1;
            ++state->edge_count[kind - 1];
            return 1;
        }
        if (entry->from == from && entry->to == to)
            return 1;
        index = (index + 1) & (state->edge_capacity - 1);
    }
}

void differential_block_coverage_callback(void *engine, uint64_t address, uint32_t size,
                                          void *opaque)
{
    BlockCoverage *state = (BlockCoverage *)opaque;
    uint32_t cpsr = 0;
    unsigned int thumb = 0;
    size_t index;
    Block *entry;
    int kind;
    if (!state)
        return;
    if (state->error)
    {
        state->stop(engine);
        return;
    }
    if (address < state->begin || address >= state->end || !size || size > state->end - address)
    {
        fail(state, engine, BLOCK_BAD_RANGE, 0);
        return;
    }
    if (state->previous_kind &&
        !insert_edge(state, state->previous_kind, state->previous_branch, address))
    {
        fail(state, engine, EDGE_CAPACITY_ERROR, (int)state->previous_kind);
        return;
    }
    if (state->cpsr_register >= 0)
    {
        int error = state->read_register(engine, state->cpsr_register, &cpsr);
        if (error)
        {
            fail(state, engine, BLOCK_REGISTER_ERROR, error);
            return;
        }
        thumb = (cpsr >> 5) & 1;
    }
    index = (size_t)mix(address ^ ((uint64_t)size << 32) ^ ((uint64_t)thumb << 63)) &
            (state->block_capacity - 1);
    for (;;)
    {
        entry = &state->blocks[index];
        if (!entry->occupied)
        {
            uint64_t branch = 0;
            if (state->block_count >= state->block_capacity - state->block_capacity / 4)
            {
                fail(state, engine, BLOCK_CAPACITY_ERROR, 0);
                return;
            }
            kind = state->classify(address, size, thumb, &branch);
            if (kind < 0 || kind > 2 || (kind && (branch < address || branch >= address + size)))
            {
                fail(state, engine, BLOCK_CLASSIFIER_ERROR, kind);
                return;
            }
            entry->address = address;
            entry->size = size;
            entry->thumb = (unsigned char)thumb;
            entry->branch = branch;
            entry->kind = (unsigned char)kind;
            entry->occupied = 1;
            ++state->block_count;
            break;
        }
        if (entry->address == address && entry->size == size && entry->thumb == thumb)
            break;
        index = (index + 1) & (state->block_capacity - 1);
    }
    state->previous_branch = entry->branch;
    state->previous_kind = entry->kind;
}

void differential_block_coverage_reset(void *opaque)
{
    BlockCoverage *state = (BlockCoverage *)opaque;
    if (state)
        state->previous_kind = 0;
}

int differential_block_coverage_error(void *opaque, int *detail)
{
    BlockCoverage *state = (BlockCoverage *)opaque;
    if (!state)
        return -1;
    if (detail)
        *detail = state->detail;
    return state->error;
}

size_t differential_block_coverage_count(void *opaque, unsigned int kind)
{
    BlockCoverage *state = (BlockCoverage *)opaque;
    if (!state || kind > 2)
        return 0;
    return kind ? state->edge_count[kind - 1] : state->block_count;
}

/* kind=0 writes addresses; kinds 1/2 write interleaved from/to pairs.
 * Capacity is measured in blocks/edges, not in uint64_t words. */
size_t differential_block_coverage_export(void *opaque, unsigned int kind,
                                          uint64_t *output, size_t capacity)
{
    BlockCoverage *state = (BlockCoverage *)opaque;
    size_t index, count = 0;
    size_t required = differential_block_coverage_count(opaque, kind);
    if (!state || kind > 2 || capacity < required || (!output && required))
        return SIZE_MAX;
    if (!kind)
    {
        for (index = 0; index < state->block_capacity; ++index)
            if (state->blocks[index].occupied)
                output[count++] = state->blocks[index].address;
    }
    else
    {
        for (index = 0; index < state->edge_capacity; ++index)
            if (state->edges[kind - 1][index].occupied)
            {
                output[2 * count] = state->edges[kind - 1][index].from;
                output[2 * count + 1] = state->edges[kind - 1][index].to;
                ++count;
            }
    }
    return count;
}
