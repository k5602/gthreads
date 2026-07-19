#include <string.h>

#include "runtime_state.h"

gth_status_t gth_context_init_thread(gth_ctx_t *ctx, const gth_stack_allocation_t *stack,
                                     size_t slot_index)
{
    if (ctx == NULL || stack == NULL)
    {
        return GTH_EINVAL;
    }

    memset(ctx->fxsave_buf, 0, sizeof(ctx->fxsave_buf));
    ctx->fxsave_area = ctx->fxsave_buf;

    /*
     * stack_top is page-aligned (set by gth_stack_allocate), so it is
     * 16-byte aligned. gth_ctx_make will subtract 16 for trampoline
     * args, keeping 16-byte alignment.
     */
    gth_ctx_make(ctx, stack->stack_top, gth_context_thread_trampoline, slot_index);

    return GTH_OK;
}

void gth_context_destroy(gth_ctx_t *ctx)
{
    if (ctx != NULL)
    {
        ctx->fxsave_area = NULL;
    }
}

void gth_context_thread_trampoline(size_t slot_index_arg)
{
    gth_runtime_state_t *state = gth_runtime_state();
    size_t idx = slot_index_arg;
    gth_thread_record_t *thread = &state->threads[idx];

    void *result = thread->fn(thread->arg);

    if (thread->state != GTH_THREAD_CANCELED)
    {
        thread->retval = result;
        thread->state = GTH_THREAD_DONE;
    }
    else if (thread->state == GTH_THREAD_CANCELED)
    {
        thread->retval = NULL;
    }

    gth_trace_thread_exit(thread->tid, thread->retval);

    state->current_tid = 0U;

    /*
     * Thread is done; switch back to the scheduler. The current thread's
     * context is discarded (it will never be resumed), so we pass a
     * zeroed dummy as the `from` context. gth_ctx_swap checks for NULL
     * fxsave_area before executing fxsave, so this is safe.
     */
    gth_ctx_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.fxsave_area = NULL;
    gth_ctx_swap(&dummy, &state->scheduler_ctx);
}
