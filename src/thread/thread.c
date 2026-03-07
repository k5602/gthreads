#include <stddef.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

gth_status_t gth_thread_create(gth_tid_t *out_tid, const gth_thread_attr_t *attr, gth_thread_fn fn,
                               void *arg)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }
    if (out_tid == NULL || fn == NULL)
    {
        return GTH_EINVAL;
    }

    gth_thread_record_t *slot = gth_runtime_alloc_thread_slot(state);
    if (slot == NULL)
    {
        return GTH_ENOMEM;
    }

    slot->tid = state->next_tid++;
    slot->fn = fn;
    slot->arg = arg;
    slot->retval = NULL;
    slot->state = GTH_THREAD_READY;
    slot->priority = (attr != NULL) ? attr->priority : 0U;

    state->runnable_threads += 1U;
    *out_tid = slot->tid;
    return GTH_OK;
}

gth_status_t gth_thread_yield(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    gth_status_t status = gth_scheduler_run_next();
    return (status == GTH_ENOTFOUND) ? GTH_OK : status;
}

gth_status_t gth_thread_join(gth_tid_t tid, void **retval)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (tid == state->current_tid && tid != 0U)
    {
        return GTH_ESTATE;
    }

    gth_thread_record_t *thread = gth_runtime_find_thread(state, tid);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }

    if (!gth_thread_is_terminal(thread->state))
    {
        gth_status_t run_status = gth_scheduler_run_until(tid);
        if (run_status != GTH_OK)
        {
            return run_status;
        }
        thread = gth_runtime_find_thread(state, tid);
        if (thread == NULL)
        {
            return GTH_ENOTFOUND;
        }
    }

    if (retval != NULL)
    {
        *retval = thread->retval;
    }

    thread->tid = 0;
    thread->fn = NULL;
    thread->arg = NULL;
    thread->retval = NULL;
    thread->state = GTH_THREAD_EMPTY;
    thread->priority = 0U;
    return GTH_OK;
}

gth_status_t gth_thread_cancel(gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    gth_thread_record_t *thread = gth_runtime_find_thread(state, tid);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }
    if (gth_thread_is_terminal(thread->state))
    {
        return GTH_ESTATE;
    }
    if (thread->state == GTH_THREAD_READY && state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }
    if (thread->state == GTH_THREAD_BLOCKED && state->blocked_threads > 0U)
    {
        state->blocked_threads -= 1U;
    }

    thread->retval = NULL;
    thread->state = GTH_THREAD_CANCELED;
    return GTH_OK;
}

gth_tid_t gth_thread_self(void)
{
    return gth_runtime_state()->current_tid;
}
