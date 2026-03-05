#include <stddef.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

static gth_thread_record_t *gth_find_thread(gth_runtime_state_t *state, gth_tid_t tid)
{
    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state != GTH_THREAD_EMPTY && state->threads[i].tid == tid)
        {
            return &state->threads[i];
        }
    }
    return NULL;
}

static gth_thread_record_t *gth_alloc_thread_slot(gth_runtime_state_t *state)
{
    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state == GTH_THREAD_EMPTY)
        {
            return &state->threads[i];
        }
    }
    return NULL;
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

    gth_thread_record_t *slot = gth_alloc_thread_slot(state);
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
    state->context_switches += 1U;
    return GTH_OK;
}

gth_status_t gth_thread_join(gth_tid_t tid, void **retval)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    gth_thread_record_t *thread = gth_find_thread(state, tid);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }
    if (thread->state != GTH_THREAD_DONE && thread->state != GTH_THREAD_CANCELED)
    {
        return GTH_EBUSY;
    }
    if (retval != NULL)
    {
        *retval = thread->retval;
    }
    thread->state = GTH_THREAD_EMPTY;
    return GTH_OK;
}

gth_status_t gth_thread_cancel(gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    gth_thread_record_t *thread = gth_find_thread(state, tid);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }
    if (thread->state == GTH_THREAD_DONE || thread->state == GTH_THREAD_CANCELED)
    {
        return GTH_ESTATE;
    }
    if (state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }
    thread->state = GTH_THREAD_CANCELED;
    return GTH_OK;
}

gth_tid_t gth_thread_self(void)
{
    return gth_runtime_state()->current_tid;
}
