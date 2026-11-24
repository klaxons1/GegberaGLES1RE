/* ARM predicate outcomes without a Python callback for every instruction.
 * The driver supplies Unicorn's register reader and CPSR register identifier,
 * so this optional helper needs no Unicorn headers or link-time dependency.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int (*DifferentialRegisterRead)(void *engine, int reg, void *value);

typedef struct DifferentialARMCoverage
{
    uint64_t base;
    size_t slots;
    DifferentialRegisterRead read_register;
    int cpsr_register;
    int error;
    unsigned int outcome_count;
    unsigned char *conditions;
    unsigned char *outcomes;
} DifferentialARMCoverage;

enum
{
    ARM_CONDITION_ALWAYS = 14,
    ARM_CONDITION_UNREGISTERED = 255,
    ARM_PREDICATE_FALSE = 1,
    ARM_PREDICATE_TRUE = 2,
    ARM_CPSR_THUMB = 1 << 5
};

void *differential_arm_coverage_create(uint64_t base, size_t size,
                                     DifferentialRegisterRead read_register,
                                     int cpsr_register)
{
    DifferentialARMCoverage *coverage;
    size_t slots;
    if ((base & 3u) || !size || size > SIZE_MAX - 3 || !read_register)
        return NULL;
    slots = (size + 3) / 4;
    coverage = (DifferentialARMCoverage *)calloc(1, sizeof(*coverage));
    if (!coverage)
        return NULL;
    coverage->conditions = (unsigned char *)malloc(slots);
    coverage->outcomes = (unsigned char *)calloc(slots, 1);
    if (!coverage->conditions || !coverage->outcomes)
    {
        free(coverage->conditions);
        free(coverage->outcomes);
        free(coverage);
        return NULL;
    }
    memset(coverage->conditions, ARM_CONDITION_UNREGISTERED, slots);
    coverage->base = base;
    coverage->slots = slots;
    coverage->read_register = read_register;
    coverage->cpsr_register = cpsr_register;
    return coverage;
}

static int coverage_index(DifferentialARMCoverage *coverage, uint64_t address,
                          size_t *index)
{
    uint64_t offset;
    if (!coverage || address < coverage->base)
        return 0;
    offset = address - coverage->base;
    if ((offset & 3u) || offset / 4 >= coverage->slots)
        return 0;
    *index = (size_t)(offset / 4);
    return 1;
}

/* condition is the architectural instruction[31:28] nibble, not Capstone's
 * condition enum. AL and the unconditional extension encoding are excluded.
 */
int differential_arm_coverage_add(void *opaque, uint64_t address, unsigned int condition)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    size_t index;
    if (condition >= ARM_CONDITION_ALWAYS || !coverage_index(coverage, address, &index))
        return -1;
    if (coverage->conditions[index] != ARM_CONDITION_UNREGISTERED &&
        coverage->conditions[index] != condition)
        return -1;
    coverage->conditions[index] = (unsigned char)condition;
    return 0;
}

/* UC_HOOK_CODE runs before execution, including when an ARM predicate fails.
 * The driver verifies that property with a CMP/MOVEQ/MOVNE probe before use.
 */
void differential_arm_coverage_callback(void *engine, uint64_t address, uint32_t size,
                                        void *opaque)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    size_t index;
    uint32_t cpsr = 0;
    unsigned int condition, n, z, c, v, passed;
    int error;
    if (size != 4 || !coverage_index(coverage, address, &index))
        return;
    condition = coverage->conditions[index];
    if (condition >= ARM_CONDITION_ALWAYS)
        return;
    /* Both outcomes are permanent until reset; subsequent executions cannot
     * add coverage and need no register read or predicate evaluation. */
    if (coverage->outcomes[index] == (ARM_PREDICATE_FALSE | ARM_PREDICATE_TRUE))
        return;
    error = coverage->read_register(engine, coverage->cpsr_register, &cpsr);
    if (error)
    {
        coverage->error = error;
        return;
    }
    if (cpsr & ARM_CPSR_THUMB)
        return;
    n = cpsr >> 31;
    z = (cpsr >> 30) & 1u;
    c = (cpsr >> 29) & 1u;
    v = (cpsr >> 28) & 1u;
    switch (condition >> 1)
    {
    case 0: passed = z; break;             /* EQ / NE */
    case 1: passed = c; break;             /* CS / CC */
    case 2: passed = n; break;             /* MI / PL */
    case 3: passed = v; break;             /* VS / VC */
    case 4: passed = c && !z; break;       /* HI / LS */
    case 5: passed = n == v; break;        /* GE / LT */
    default: passed = !z && n == v; break; /* GT / LE */
    }
    passed ^= condition & 1u;
    passed = passed ? ARM_PREDICATE_TRUE : ARM_PREDICATE_FALSE;
    if (!(coverage->outcomes[index] & passed))
        ++coverage->outcome_count;
    coverage->outcomes[index] |= passed;
}

unsigned int differential_arm_coverage_count(void *opaque)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    return coverage ? coverage->outcome_count : 0;
}

unsigned int differential_arm_coverage_get(void *opaque, uint64_t address)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    size_t index;
    return coverage_index(coverage, address, &index) ? coverage->outcomes[index] : 0;
}

int differential_arm_coverage_error(void *opaque)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    return coverage ? coverage->error : -1;
}

void differential_arm_coverage_reset(void *opaque)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    if (coverage)
    {
        memset(coverage->outcomes, 0, coverage->slots);
        coverage->error = 0;
        coverage->outcome_count = 0;
    }
}

/* Remove the emulator hook before destroying its context. */
void differential_arm_coverage_destroy(void *opaque)
{
    DifferentialARMCoverage *coverage = (DifferentialARMCoverage *)opaque;
    if (coverage)
    {
        free(coverage->conditions);
        free(coverage->outcomes);
        free(coverage);
    }
}
