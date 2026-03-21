#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "runtime_state.h"

static size_t gth_page_size(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
    {
        return 0U;
    }

    return (size_t)page_size;
}

static int gth_is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

static size_t gth_align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

static void gth_stack_clear(gth_stack_allocation_t *stack)
{
    if (stack == NULL)
    {
        return;
    }

    stack->memory = NULL;
    stack->total_size = 0U;
    stack->stack_size = 0U;
    stack->stack_top = NULL;
    stack->guard_page = NULL;
}

gth_status_t gth_stack_allocate(size_t stack_size_bytes, gth_stack_allocation_t *out_stack)
{
    if (out_stack == NULL)
    {
        return GTH_EINVAL;
    }

    gth_stack_clear(out_stack);

    const size_t page_size = gth_page_size();
    if (page_size == 0U || !gth_is_power_of_two(page_size))
    {
        return GTH_EINTERNAL;
    }

    if (stack_size_bytes < 16U * 1024U)
    {
        return GTH_EINVAL;
    }

    const size_t aligned_stack_size = gth_align_up(stack_size_bytes, page_size);
    if (aligned_stack_size < stack_size_bytes)
    {
        return GTH_EINVAL;
    }

    const size_t total_size = aligned_stack_size + page_size;
    if (total_size < aligned_stack_size)
    {
        return GTH_EINVAL;
    }

    void *memory =
        mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
    {
        return GTH_ENOMEM;
    }

    if (mprotect(memory, page_size, PROT_NONE) != 0)
    {
        (void)munmap(memory, total_size);
        return GTH_EINTERNAL;
    }

    out_stack->memory = memory;
    out_stack->total_size = total_size;
    out_stack->stack_size = aligned_stack_size;
    out_stack->guard_page = memory;
    out_stack->stack_top = (void *)((uintptr_t)memory + total_size);

    return GTH_OK;
}

void gth_stack_free(gth_stack_allocation_t *stack)
{
    if (stack == NULL)
    {
        return;
    }

    if (stack->memory != NULL && stack->total_size != 0U)
    {
        (void)munmap(stack->memory, stack->total_size);
    }

    gth_stack_clear(stack);
}
