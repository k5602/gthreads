#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"
#include "wait_queue.h"

#define GTH_SEM_WAIT_QUEUE_CAPACITY 52U

typedef struct
{
    uint32_t initialized;
    uint32_t count;
    gth_wait_queue_t wq;
} gth_sem_impl_t;
_Static_assert(sizeof(gth_sem_impl_t) <= sizeof(gth_sem_t), "sem impl must fit in opaque storage");

static gth_sem_impl_t *gth_sem_impl(gth_sem_t *s)
{
    return (gth_sem_impl_t *)s;
}

static const gth_sem_impl_t *gth_sem_cimpl(const gth_sem_t *s)
{
    return (const gth_sem_impl_t *)s;
}

gth_status_t gth_sem_init(gth_sem_t *s, uint32_t initial_count)
{
    gth_sem_impl_t *impl = NULL;

    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    memset(s, 0, sizeof(*s));
    impl = gth_sem_impl(s);
    impl->initialized = 1U;
    impl->count = initial_count;
    gth_wq_init(&impl->wq, (uint8_t)GTH_SEM_WAIT_QUEUE_CAPACITY);
    return GTH_OK;
}

gth_status_t gth_sem_wait(gth_sem_t *s)
{
    gth_sem_impl_t *impl = NULL;
    size_t my_slot;

    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_sem_impl(s);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->count == 0U)
    {
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
        gth_thread_block();
    }
    else
    {
        impl->count -= 1U;
    }
    return GTH_OK;
}

gth_status_t gth_sem_post(gth_sem_t *s)
{
    gth_sem_impl_t *impl = NULL;

    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    impl = gth_sem_impl(s);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (!gth_wq_is_empty(&impl->wq))
    {
        uint8_t waiter_slot = gth_wq_dequeue(&impl->wq);
        if (waiter_slot != 0xFF)
        {
            gth_thread_unblock_slot((size_t)waiter_slot);
        }
    }
    else
    {
        impl->count += 1U;
    }

    return GTH_OK;
}

gth_status_t gth_sem_destroy(gth_sem_t *s)
{
    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    if (gth_sem_cimpl(s)->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    memset(s, 0, sizeof(*s));
    return GTH_OK;
}
