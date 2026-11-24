/* Conservative instruction limits without a callback on every instruction. */
#include <stdint.h>
#include <stdlib.h>

typedef int (*DifferentialEmulationStop)(void *engine);
typedef int (*DifferentialRegisterRead)(void *engine, int reg, void *value);

typedef struct DifferentialBudget
{
    DifferentialEmulationStop stop;
    DifferentialRegisterRead read_register;
    uint64_t remaining;
    uint64_t used;
    unsigned int minimum_instruction_bytes;
    int cpsr_register;
    int active;
    int exhausted;
    int error;
} DifferentialBudget;

void *differential_budget_create(DifferentialEmulationStop stop,
                                 unsigned int minimum_instruction_bytes,
                                 DifferentialRegisterRead read_register,
                                 int cpsr_register)
{
    DifferentialBudget *budget;
    if (!stop || (minimum_instruction_bytes != 1 && minimum_instruction_bytes != 2)
        || (minimum_instruction_bytes == 2 && !read_register))
        return NULL;
    budget = (DifferentialBudget *)calloc(1, sizeof(*budget));
    if (budget)
    {
        budget->stop = stop;
        budget->minimum_instruction_bytes = minimum_instruction_bytes;
        budget->read_register = read_register;
        budget->cpsr_register = cpsr_register;
    }
    return budget;
}

void differential_budget_begin(void *opaque, uint64_t limit)
{
    DifferentialBudget *budget = (DifferentialBudget *)opaque;
    budget->remaining = limit;
    budget->used = 0;
    budget->active = 1;
    budget->exhausted = 0;
    budget->error = 0;
}

void differential_budget_callback(void *engine, uint64_t address, uint32_t size,
                                  void *opaque)
{
    DifferentialBudget *budget = (DifferentialBudget *)opaque;
    uint64_t charge;
    unsigned int instruction_bytes = budget->minimum_instruction_bytes;
    (void)address;
    if (!budget->active)
        return;
    if (instruction_bytes == 2)
    {
        uint32_t cpsr = 0;
        int error = budget->read_register(engine, budget->cpsr_register, &cpsr);
        if (error)
        {
            budget->error = error;
            budget->stop(engine);
            return;
        }
        instruction_bytes = (cpsr & (1u << 5)) ? 2u : 4u;
    }
    /* ARM blocks have exactly size/4 instructions. Thumb blocks have at most
     * ceil(size/2), and x86 blocks at most size. Charge before execution. REP
     * iterations reenter the block hook, so their repetitions are bounded too.
     * Treat an unexpected empty block as one unit to guarantee progress. */
    charge = ((uint64_t)size + instruction_bytes - 1) / instruction_bytes;
    if (!charge)
        charge = 1;
    if (charge > budget->remaining)
    {
        budget->exhausted = 1;
        budget->error = budget->stop(engine);
        return;
    }
    budget->remaining -= charge;
    budget->used += charge;
}

int differential_budget_end(void *opaque)
{
    DifferentialBudget *budget = (DifferentialBudget *)opaque;
    budget->active = 0;
    return budget->error ? -budget->error : budget->exhausted;
}

uint64_t differential_budget_used(void *opaque)
{
    return ((DifferentialBudget *)opaque)->used;
}

/* Remove the emulator hook before destroying its context. */
void differential_budget_destroy(void *opaque)
{
    free(opaque);
}
