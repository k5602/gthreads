#include <string.h>

#include "gthreads/gthreads.h"

typedef struct
{
    uint32_t initialized;
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
    return GTH_OK;
}

gth_status_t gth_cond_wait(gth_cond_t *c, gth_mutex_t *m)
{
    if (c == NULL || m == NULL)
    {
        return GTH_EINVAL;
    }
    if (gth_cond_impl(c)->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    gth_status_t unlock_status = gth_mutex_unlock(m);
    if (unlock_status != GTH_OK)
    {
        return unlock_status;
    }
    return gth_mutex_lock(m);
}

gth_status_t gth_cond_signal(gth_cond_t *c)
{
    if (c == NULL)
    {
        return GTH_EINVAL;
    }
    return (gth_cond_impl(c)->initialized != 0U) ? GTH_OK : GTH_ESTATE;
}

gth_status_t gth_cond_broadcast(gth_cond_t *c)
{
    return gth_cond_signal(c);
}

gth_status_t gth_cond_destroy(gth_cond_t *c)
{
    if (c == NULL)
    {
        return GTH_EINVAL;
    }
    if (gth_cond_impl(c)->initialized == 0U)
    {
        return GTH_ESTATE;
    }
    memset(c, 0, sizeof(*c));
    return GTH_OK;
}
