#include <stddef.h>
#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"
#include "wait_queue.h"

typedef struct
{
    uint32_t initialized;
    uint32_t locked;
    gth_tid_t owner;
    gth_wait_queue_t wq;
} gth_mutex_impl_t;

_Static_assert(sizeof(gth_mutex_impl_t) <= sizeof(gth_mutex_t),
               "mutex impl must fit in opaque storage");

static gth_mutex_impl_t *gth_mutex_impl(gth_mutex_t *m)
{
    return (gth_mutex_impl_t *)m;
}

static const gth_mutex_impl_t *gth_mutex_cimpl(const gth_mutex_t *m)
{
    return (const gth_mutex_impl_t *)m;
}

gth_status_t gth_mutex_init(gth_mutex_t *m)
{
    gth_mutex_impl_t *impl = NULL;

    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    memset(m, 0, sizeof(*m));
    impl = gth_mutex_impl(m);
    impl->initialized = 1U;
    impl->locked = 0U;
    impl->owner = 0U;
    gth_wq_init(&impl->wq, (uint8_t)GTH_WQ_FIXED_SLOTS);
    return GTH_OK;
}

gth_status_t gth_mutex_lock(gth_mutex_t *m)
{
    gth_mutex_impl_t *impl = NULL;
    size_t my_slot;

    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_mutex_impl(m);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->locked != 0U)
    {
        if (impl->owner == gth_thread_self() && impl->owner != 0U)
        {
            return GTH_EBUSY;
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

        gth_wq_enqueue(&impl->wq, (uint8_t)my_slot);
        gth_trace_mutex_wait(gth_thread_self(), (const void *)m);
        gth_thread_block();

        while (impl->locked != 0U)
        {
            gth_wq_enqueue(&impl->wq, (uint8_t)my_slot);
            gth_thread_block();
        }
    }

    impl->locked = 1U;
    impl->owner = gth_thread_self();
    gth_trace_mutex_lock(gth_thread_self(), (const void *)m);
    return GTH_OK;
}

gth_status_t gth_mutex_trylock(gth_mutex_t *m)
{
    gth_mutex_impl_t *impl = NULL;

    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_mutex_impl(m);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->locked != 0U)
    {
        return GTH_EBUSY;
    }

    impl->locked = 1U;
    impl->owner = gth_thread_self();
    return GTH_OK;
}

gth_status_t gth_mutex_unlock(gth_mutex_t *m)
{
    gth_mutex_impl_t *impl = NULL;

    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_mutex_impl(m);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->locked == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->owner != 0U && impl->owner != gth_thread_self())
    {
        return GTH_ESTATE;
    }

    impl->locked = 0U;
    impl->owner = 0U;

    if (!gth_wq_is_empty(&impl->wq))
    {
        uint8_t waiter_slot = gth_wq_dequeue(&impl->wq);
        if (waiter_slot != 0xFF)
        {
            gth_trace_mutex_wake(gth_thread_self(), (const void *)m);
            gth_thread_unblock_slot((size_t)waiter_slot);
        }
    }

    gth_trace_mutex_unlock(gth_thread_self(), (const void *)m);
    return GTH_OK;
}

gth_status_t gth_mutex_destroy(gth_mutex_t *m)
{
    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    const gth_mutex_impl_t *impl = gth_mutex_cimpl(m);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->locked != 0U)
    {
        return GTH_EBUSY;
    }

    if (!gth_wq_is_empty(&impl->wq))
    {
        return GTH_EBUSY;
    }

    memset(m, 0, sizeof(*m));
    return GTH_OK;
}
