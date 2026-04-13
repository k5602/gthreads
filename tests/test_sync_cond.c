#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"
#include "test_support.h"

static gth_mutex_t g_cond_mutex;
static gth_cond_t g_cond_var;
static volatile int g_cond_flag;
static volatile int g_cond_wake_order[8];
static volatile int g_cond_wake_idx;

static inline void *cond_signaler_fn(void *arg)
{
    (void)arg;
    gth_mutex_lock(&g_cond_mutex);
    g_cond_flag = 1;
    gth_cond_signal(&g_cond_var);
    gth_mutex_unlock(&g_cond_mutex);
    return NULL;
}

static void *cond_waiter_fn(void *arg)
{
    (void)arg;
    gth_mutex_lock(&g_cond_mutex);
    while (g_cond_flag == 0)
    {
        gth_cond_wait(&g_cond_var, &g_cond_mutex);
    }
    g_cond_flag = 2;
    gth_mutex_unlock(&g_cond_mutex);
    return NULL;
}

void test_cond_wait_signal_single(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(55);
    gth_tid_t waiter, signaler;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_init(&g_cond_mutex);
    assert_int_equal(rc, GTH_OK);
    rc = gth_cond_init(&g_cond_var);
    assert_int_equal(rc, GTH_OK);

    g_cond_flag = 0;

    rc = gth_thread_create(&waiter, NULL, cond_waiter_fn, NULL);
    assert_int_equal(rc, GTH_OK);

    gth_thread_yield();

    rc = gth_thread_create(&signaler, NULL, cond_signaler_fn, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(waiter, NULL);
    assert_int_equal(rc, GTH_OK);
    rc = gth_thread_join(signaler, NULL);
    assert_int_equal(rc, GTH_OK);

    assert_int_equal(g_cond_flag, 2);

    rc = gth_cond_destroy(&g_cond_var);
    assert_int_equal(rc, GTH_OK);
    rc = gth_mutex_destroy(&g_cond_mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

static void *cond_broadcast_waiter_fn(void *arg)
{
    int id = *(int *)arg;
    gth_mutex_lock(&g_cond_mutex);
    while (g_cond_flag == 0)
    {
        gth_cond_wait(&g_cond_var, &g_cond_mutex);
    }
    g_cond_wake_order[g_cond_wake_idx++] = id;
    gth_mutex_unlock(&g_cond_mutex);
    return NULL;
}

static void *cond_broadcaster_fn(void *arg)
{
    (void)arg;
    gth_mutex_lock(&g_cond_mutex);
    g_cond_flag = 1;
    gth_cond_broadcast(&g_cond_var);
    gth_mutex_unlock(&g_cond_mutex);
    return NULL;
}

void test_cond_broadcast_wakes_all(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(77);
    gth_tid_t waiters[3];
    gth_tid_t broadcaster;
    int ids[3] = {1, 2, 3};
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_init(&g_cond_mutex);
    assert_int_equal(rc, GTH_OK);
    rc = gth_cond_init(&g_cond_var);
    assert_int_equal(rc, GTH_OK);

    g_cond_flag = 0;
    g_cond_wake_idx = 0;

    for (int i = 0; i < 3; ++i)
    {
        rc = gth_thread_create(&waiters[i], NULL, cond_broadcast_waiter_fn, &ids[i]);
        assert_int_equal(rc, GTH_OK);
    }

    gth_thread_yield();

    rc = gth_thread_create(&broadcaster, NULL, cond_broadcaster_fn, NULL);
    assert_int_equal(rc, GTH_OK);

    for (int i = 0; i < 3; ++i)
    {
        rc = gth_thread_join(waiters[i], NULL);
        assert_int_equal(rc, GTH_OK);
    }
    rc = gth_thread_join(broadcaster, NULL);
    assert_int_equal(rc, GTH_OK);

    assert_int_equal(g_cond_wake_idx, 3);

    rc = gth_cond_destroy(&g_cond_var);
    assert_int_equal(rc, GTH_OK);
    rc = gth_mutex_destroy(&g_cond_mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_cond_signal_no_waiters_is_noop(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(1);
    gth_cond_t c;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_cond_init(&c);
    assert_int_equal(rc, GTH_OK);

    rc = gth_cond_signal(&c);
    assert_int_equal(rc, GTH_OK);

    rc = gth_cond_broadcast(&c);
    assert_int_equal(rc, GTH_OK);

    rc = gth_cond_destroy(&c);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_cond_rejects_null(void **state)
{
    (void)state;
    assert_int_equal(gth_cond_init(NULL), GTH_EINVAL);
    assert_int_equal(gth_cond_wait(NULL, NULL), GTH_EINVAL);
    assert_int_equal(gth_cond_signal(NULL), GTH_EINVAL);
    assert_int_equal(gth_cond_broadcast(NULL), GTH_EINVAL);
    assert_int_equal(gth_cond_destroy(NULL), GTH_EINVAL);
}
