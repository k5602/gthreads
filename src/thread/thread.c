#include <stddef.h>
#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

static void gth_runtime_account_state_change(gth_runtime_state_t *state, gth_thread_state_t from,
                                             gth_thread_state_t to)
{
    if (state == NULL || from == to)
    {
        return;
    }

    if (from == GTH_THREAD_READY && state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }
    else if (from == GTH_THREAD_BLOCKED && state->blocked_threads > 0U)
    {
        state->blocked_threads -= 1U;
    }

    if (to == GTH_THREAD_READY)
    {
        state->runnable_threads += 1U;
    }
    else if (to == GTH_THREAD_BLOCKED)
    {
        state->blocked_threads += 1U;
    }
}

static void gth_runtime_set_thread_state(gth_runtime_state_t *state, gth_thread_record_t *thread,
                                         gth_thread_state_t new_state)
{
    if (state == NULL || thread == NULL || thread->state == new_state)
    {
        return;
    }

    gth_runtime_account_state_change(state, thread->state, new_state);
    thread->state = new_state;
}

static gth_thread_record_t *gth_thread_acquire_slot(gth_runtime_state_t *state)
{
    return gth_runtime_alloc_thread_slot(state);
}

static void gth_thread_release_slot(gth_runtime_state_t *state, gth_thread_record_t *thread)
{
    if (state == NULL || thread == NULL)
    {
        return;
    }

    if (thread->state == GTH_THREAD_READY && state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }
    else if (thread->state == GTH_THREAD_BLOCKED && state->blocked_threads > 0U)
    {
        state->blocked_threads -= 1U;
    }

    memset(thread, 0, sizeof(*thread));
    thread->state = GTH_THREAD_EMPTY;
}

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

    gth_thread_record_t *slot = gth_thread_acquire_slot(state);
    if (slot == NULL)
    {
        return GTH_ENOMEM;
    }

    memset(slot, 0, sizeof(*slot));
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

    return gth_scheduler_run_next();
}

gth_status_t gth_thread_join(gth_tid_t tid, void **retval)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }
    if (tid == 0U)
    {
        return GTH_EINVAL;
    }
    if (tid == state->current_tid)
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
        gth_status_t status = gth_scheduler_run_until(tid);
        if (status != GTH_OK)
        {
            return status;
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

    gth_thread_release_slot(state, thread);
    return GTH_OK;
}

gth_status_t gth_thread_cancel(gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }
    if (tid == 0U)
    {
        return GTH_EINVAL;
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

    gth_runtime_set_thread_state(state, thread, GTH_THREAD_CANCELED);
    thread->retval = NULL;
    return GTH_OK;
}

gth_tid_t gth_thread_self(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (state == NULL || !state->initialized)
    {
        return 0U;
    }
    return state->current_tid;
}
