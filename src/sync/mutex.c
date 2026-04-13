#include <stddef.h>
#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

#define GTH_MUTEX_WQ_CAPACITY 44U

typedef struct
{
    uint32_t initialized;
    uint32_t locked;
    gth_tid_t owner;
    uint8_t wq_head;
    uint8_t wq_tail;
    uint8_t wq_count;
    uint8_t wq_capacity;
    uint8_t wq_slots[GTH_MUTEX_WQ_CAPACITY];
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

static int gth_mutex_wq_is_empty(const gth_mutex_impl_t *impl)
{
    return impl->wq_count == 0;
}

static int gth_mutex_wq_is_full(const gth_mutex_impl_t *impl)
{
    return impl->wq_count >= impl->wq_capacity;
}

static void gth_mutex_wq_enqueue(gth_mutex_impl_t *impl, uint8_t slot)
{
    if (impl->wq_count >= impl->wq_capacity)
    {
        return;
    }
    impl->wq_slots[impl->wq_tail] = slot;
    impl->wq_tail = (impl->wq_tail + 1) % impl->wq_capacity;
    impl->wq_count += 1;
}

static uint8_t gth_mutex_wq_dequeue(gth_mutex_impl_t *impl)
{
    uint8_t slot;
    if (impl->wq_count == 0)
    {
        return 0xFF;
    }
    slot = impl->wq_slots[impl->wq_head];
    impl->wq_head = (impl->wq_head + 1) % impl->wq_capacity;
    impl->wq_count -= 1;
    return slot;
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
    impl->wq_head = 0;
    impl->wq_tail = 0;
    impl->wq_count = 0;
    impl->wq_capacity = GTH_MUTEX_WQ_CAPACITY;
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

        if (gth_mutex_wq_is_full(impl))
        {
            return GTH_EBUSY;
        }

        gth_mutex_wq_enqueue(impl, (uint8_t)my_slot);
        gth_thread_block();

        while (impl->locked != 0U)
        {
            gth_mutex_wq_enqueue(impl, (uint8_t)my_slot);
            gth_thread_block();
        }
    }

    impl->locked = 1U;
    impl->owner = gth_thread_self();
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

    if (!gth_mutex_wq_is_empty(impl))
    {
        uint8_t waiter_slot = gth_mutex_wq_dequeue(impl);
        if (waiter_slot != 0xFF)
        {
            gth_thread_unblock_slot((size_t)waiter_slot);
        }
    }

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

    if (!gth_mutex_wq_is_empty(impl))
    {
        return GTH_EBUSY;
    }

    memset(m, 0, sizeof(*m));
    return GTH_OK;
}
