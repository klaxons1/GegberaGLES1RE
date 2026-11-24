/* Clipper records contain no pointers; the owning state supplies their buffer. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gerbera/grState.h"

enum { DIFF_CLIP_STACK_BYTES = 1332 + 104 * 128 };
typedef char clip_vertex_layout[(sizeof(_grVertex) == 128) ? 1 : -1];

int differential_clipper(const void *input, const void *planes, uint32_t indices,
                         void *output, unsigned char *fan)
{
    Gerbera_s *state = (Gerbera_s *)calloc(1, sizeof(*state));
    unsigned char *stack = (unsigned char *)malloc(DIFF_CLIP_STACK_BYTES);
    int count;
    if (state == NULL || stack == NULL)
    {
        free(state);
        free(stack);
        return -1;
    }
    memcpy(stack, input, DIFF_CLIP_STACK_BYTES);
    memcpy(state->userClipPlanes, planes, sizeof(state->userClipPlanes));
    state->renderStack = stack;
    count = grClipper(state, fan, indices);
    memcpy(output, stack, DIFF_CLIP_STACK_BYTES);
    free(stack);
    free(state);
    return count;
}
