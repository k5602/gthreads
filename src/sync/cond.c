#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"
#include "wait_queue.h"

#define GTH_COND_WAIT_QUEUE_CAPACITY 52U

typedef struct
{
    uint32_t initialized;
    uint32_t pad;
    gth_wait_queue_t wq;
} gth_cond_impl_t;

_Static_assert(sizeof(gth_cond_impl_t) <= sizeof(gth_cond_t),
               "cond impl must fit in opaque storage");

static gth_cond_impl_t *gth_cond_impl(gth_cond_t *c)
{
    return (gth_cond_impl_t *)c;
}

gth_status_t gth_cond_init(gth_cond_t *c)
{
    gth_cond_impl_t *impl = NULL;

    if (c == NULL)
    {
        return GTH_EINVAL;
    }

    memset(c, 0, sizeof(*c));
    impl = gth_cond_impl(c);
    impl->initialized = 1U;
    impl->pad = 0U;
    gth_wq_init(&impl->wq, (uint8_t)GTH_COND_WAIT_QUEUE_CAPACITY);
    return GTH_OK;
}

gth_status_t gth_cond_wait(gth_cond_t *c, gth_mutex_t *m)
{
    gth_cond_impl_t *impl = NULL;
    size_t my_slot;
    gth_status_t status;

    if (c == NULL || m == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_cond_impl(c);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    my_slot = gth_thread_current_slot_index();
    if (my_slot >= GTH_MAX_THREADS)
    {
        return GTH_ESTATE;
    }

    if (gth_wq_is_full(&impl->wq))
    {
        return GTH_EBUSY;
    }

    gth_runtime_state_t *state = gth_runtime_state();
    gth_thread_record_t *current = &state->threads[my_slot];

    current->state = GTH_THREAD_BLOCKED;
    state->blocked_threads += 1U;
    gth_wq_enqueue(&impl->wq, (uint8_t)my_slot);
    gth_trace_cond_wait(gth_thread_self(), (const void *)c);

    status = gth_mutex_unlock(m);
    if (status != GTH_OK)
    {
        current->state = GTH_THREAD_RUNNING;
        if (state->blocked_threads > 0U)
        {
            state->blocked_threads -= 1U;
        }
        gth_wq_remove(&impl->wq, (uint8_t)my_slot);
        return status;
    }

    gth_ctx_swap(&current->ctx, &state->scheduler_ctx);

    status = gth_mutex_lock(m);
    return status;
}

gth_status_t gth_cond_signal(gth_cond_t *c)
{
    gth_cond_impl_t *impl = NULL;

    if (c == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_cond_impl(c);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (!gth_wq_is_empty(&impl->wq))
    {
        uint8_t waiter_slot = gth_wq_dequeue(&impl->wq);
        if (waiter_slot != 0xFF)
        {
            gth_tid_t target_tid = gth_runtime_state()->threads[waiter_slot].tid;
            gth_trace_cond_signal(gth_thread_self(), (const void *)c, target_tid);
            gth_thread_unblock_slot((size_t)waiter_slot);
        }
    }

    return GTH_OK;
}

gth_status_t gth_cond_broadcast(gth_cond_t *c)
{
    gth_cond_impl_t *impl = NULL;

    if (c == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_cond_impl(c);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    uint32_t wake_count = 0;
    while (!gth_wq_is_empty(&impl->wq))
    {
        uint8_t waiter_slot = gth_wq_dequeue(&impl->wq);
        if (waiter_slot != 0xFF)
        {
            wake_count++;
            gth_thread_unblock_slot((size_t)waiter_slot);
        }
    }
    gth_trace_cond_broadcast(gth_thread_self(), (const void *)c, wake_count);

    return GTH_OK;
}

gth_status_t gth_cond_destroy(gth_cond_t *c)
{
    if (c == NULL)
    {
        return GTH_EINVAL;
    }

    gth_cond_impl_t *impl = gth_cond_impl(c);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (!gth_wq_is_empty(&impl->wq))
    {
        return GTH_EBUSY;
    }

    memset(c, 0, sizeof(*c));
    return GTH_OK;
}
