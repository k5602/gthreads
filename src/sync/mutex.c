#include <string.h>

#include "gthreads/gthreads.h"

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

gth_status_t gth_mutex_init(gth_mutex_t *m)
{
    if (m == NULL)
    {
        return GTH_EINVAL;
    }
    memset(m, 0, sizeof(*m));
    gth_mutex_impl(m)->initialized = 1U;
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
    impl->locked = 0U;
    impl->owner = 0;
    return GTH_OK;
}

gth_status_t gth_mutex_destroy(gth_mutex_t *m)
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
        return GTH_EBUSY;
    }
    memset(m, 0, sizeof(*m));
    return GTH_OK;
}
