#include <stddef.h>
#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

typedef struct
{
    uint32_t initialized;
    uint32_t locked;
    gth_tid_t owner;
} gth_mutex_impl_t;

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
    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    memset(m, 0, sizeof(*m));
    gth_mutex_impl(m)->initialized = 1U;
    gth_mutex_impl(m)->locked = 0U;
    gth_mutex_impl(m)->owner = 0U;
    return GTH_OK;
}

gth_status_t gth_mutex_lock(gth_mutex_t *m)
{
    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    gth_mutex_impl_t *impl = gth_mutex_impl(m);
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
        return GTH_EBUSY;
    }

    impl->locked = 1U;
    impl->owner = gth_thread_self();
    return GTH_OK;
}

gth_status_t gth_mutex_trylock(gth_mutex_t *m)
{
    return gth_mutex_lock(m);
}

gth_status_t gth_mutex_unlock(gth_mutex_t *m)
{
    if (m == NULL)
    {
        return GTH_EINVAL;
    }

    gth_mutex_impl_t *impl = gth_mutex_impl(m);
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

    memset(m, 0, sizeof(*m));
    return GTH_OK;
}
