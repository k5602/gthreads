#include <stddef.h>
#include <ucontext.h>

#include "runtime_state.h"

static gth_thread_record_t *gth_scheduler_pick_ready_thread(gth_runtime_state_t *state)
{
    gth_thread_record_t *best = NULL;

    if (state == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        gth_thread_record_t *candidate = &state->threads[i];

        if (candidate->state != GTH_THREAD_READY)
        {
            continue;
        }

        if (best == NULL)
        {
            best = candidate;
            continue;
        }

        if (state->config.policy == GTH_SCHED_PRIORITY)
        {
            if (candidate->priority > best->priority)
            {
                best = candidate;
                continue;
            }

            if (candidate->priority == best->priority && candidate->tid < best->tid)
            {
                best = candidate;
                continue;
            }
        }
        else
        {
            if (candidate->tid < best->tid)
            {
                best = candidate;
            }
        }
    }

    return best;
}

gth_status_t gth_scheduler_run_next(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_thread_record_t *thread = NULL;
    gth_tid_t previous_tid = 0U;

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    thread = gth_scheduler_pick_ready_thread(state);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }

    previous_tid = state->current_tid;

    thread->state = GTH_THREAD_RUNNING;
    state->current_tid = thread->tid;
    state->context_switches += 1U;

    if (state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }

    swapcontext(&state->scheduler_ctx, &thread->ctx);

    state->current_tid = previous_tid;

    return GTH_OK;
}

gth_status_t gth_scheduler_run_until(gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_thread_record_t *target = NULL;

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    target = gth_runtime_find_thread(state, tid);
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
