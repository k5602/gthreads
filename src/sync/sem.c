#include <string.h>

#include "gthreads/gthreads.h"

typedef struct
{
    uint32_t initialized;
    uint32_t count;
} gth_sem_impl_t;

static gth_sem_impl_t *gth_sem_impl(gth_sem_t *s)
{
    return (gth_sem_impl_t *)s;
}

gth_status_t gth_sem_init(gth_sem_t *s, uint32_t initial_count)
{
    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    memset(s, 0, sizeof(*s));
    gth_sem_impl(s)->initialized = 1U;
    gth_sem_impl(s)->count = initial_count;
    return GTH_OK;
}

gth_status_t gth_sem_wait(gth_sem_t *s)
{
    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    gth_sem_impl_t *impl = gth_sem_impl(s);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    if (impl->count == 0U)
    {
        return GTH_EBUSY;
    }

    impl->count -= 1U;
    return GTH_OK;
}

gth_status_t gth_sem_post(gth_sem_t *s)
{
    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    gth_sem_impl_t *impl = gth_sem_impl(s);
    if (impl->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    impl->count += 1U;
    return GTH_OK;
}

gth_status_t gth_sem_destroy(gth_sem_t *s)
{
    if (s == NULL)
    {
        return GTH_EINVAL;
    }

    if (gth_sem_impl(s)->initialized == 0U)
    {
        return GTH_ESTATE;
    }

    memset(s, 0, sizeof(*s));
    return GTH_OK;
}
