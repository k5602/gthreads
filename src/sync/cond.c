#include <string.h>

#include "gthreads/gthreads.h"

typedef struct
{
    uint32_t initialized;
    uint32_t waiters;
} gth_cond_impl_t;

static gth_cond_impl_t *gth_cond_impl(gth_cond_t *c)
{
    return (gth_cond_impl_t *)c;
}

gth_status_t gth_cond_init(gth_cond_t *c)
{
    if (c == NULL)
    {
        return GTH_EINVAL;
    }

    memset(c, 0, sizeof(*c));
    gth_cond_impl(c)->initialized = 1U;
    gth_cond_impl(c)->waiters = 0U;
    return GTH_OK;
}

gth_status_t gth_cond_wait(gth_cond_t *c, gth_mutex_t *m)
{
    if (c == NULL || m == NULL)
    {
        return GTH_EINVAL;
    }

    gth_cond_impl_t *impl = gth_cond_impl(c);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    /*
     * Minimal cooperative-runtime implementation:
     * - validate the condition variable and mutex
     * - release and reacquire the mutex to preserve wait protocol shape
     * - keep the operation deterministic without blocking primitives yet
     *
     * Full blocking semantics will be implemented when the scheduler
     * supports parked threads and wake queues in later Milestones.
     */
    impl->waiters += 1U;

    gth_status_t status = gth_mutex_unlock(m);
    if (status != GTH_OK)
    {
        impl->waiters -= 1U;
        return status;
    }

    status = gth_mutex_lock(m);
    impl->waiters -= 1U;
    return status;
}

gth_status_t gth_cond_signal(gth_cond_t *c)
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

    if (impl->waiters == 0U)
    {
        return GTH_OK;
    }

    impl->waiters -= 1U;
    return GTH_OK;
}

gth_status_t gth_cond_broadcast(gth_cond_t *c)
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

    impl->waiters = 0U;
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

    memset(c, 0, sizeof(*c));
    return GTH_OK;
}
