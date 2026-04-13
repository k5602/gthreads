#define _GNU_SOURCE

#include "runtime_state.h"

gth_status_t gth_context_init_thread(ucontext_t *ctx, const gth_stack_allocation_t *stack,
                                     size_t slot_index)
{
    if (getcontext(ctx) != 0)
    {
        return GTH_EINTERNAL;
    }

    ctx->uc_stack.ss_sp = stack->memory;
    ctx->uc_stack.ss_size = stack->total_size;
    ctx->uc_stack.ss_flags = 0;
    ctx->uc_link = &gth_runtime_state()->scheduler_ctx;

    makecontext(ctx, (void (*)(void))gth_context_thread_trampoline, 1, (int)slot_index);

    return GTH_OK;
}

void gth_context_thread_trampoline(int slot_index_arg)
{
    gth_runtime_state_t *state = gth_runtime_state();
    size_t idx = (size_t)slot_index_arg;
    gth_thread_record_t *thread = &state->threads[idx];

    void *result = thread->fn(thread->arg);

    if (thread->state == GTH_THREAD_RUNNING)
    {
        thread->retval = result;
        thread->state = GTH_THREAD_DONE;
    }
    else if (thread->state == GTH_THREAD_CANCELED)
    {
        thread->retval = NULL;
    }
}

gth_status_t gth_context_backend_placeholder(void)
{
    return GTH_OK;
}
