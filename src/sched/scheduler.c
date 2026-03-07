#include <stddef.h>

#include "runtime_state.h"

static gth_thread_record_t *gth_scheduler_pick_ready_thread(gth_runtime_state_t *state)
{
    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state == GTH_THREAD_READY)
        {
            return &state->threads[i];
        }
    }
    return NULL;
}

gth_status_t gth_scheduler_run_next(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    gth_thread_record_t *thread = gth_scheduler_pick_ready_thread(state);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }

    gth_tid_t previous_tid = state->current_tid;
    thread->state = GTH_THREAD_RUNNING;
    state->current_tid = thread->tid;
    state->context_switches += 1U;
    if (state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }

    void *retval = thread->fn(thread->arg);
    if (thread->state == GTH_THREAD_RUNNING)
    {
        thread->retval = retval;
        thread->state = GTH_THREAD_DONE;
    }
    else if (thread->state == GTH_THREAD_CANCELED)
    {
        thread->retval = NULL;
    }

    state->current_tid = previous_tid;
    return GTH_OK;
}

gth_status_t gth_scheduler_run_until(gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    gth_thread_record_t *target = gth_runtime_find_thread(state, tid);
    if (target == NULL)
    {
        return GTH_ENOTFOUND;
    }

    while (!gth_thread_is_terminal(target->state))
    {
        gth_status_t status = gth_scheduler_run_next();
        if (status == GTH_ENOTFOUND)
        {
            return GTH_EBUSY;
        }
        if (status != GTH_OK)
        {
            return status;
        }
    }

    return GTH_OK;
}
