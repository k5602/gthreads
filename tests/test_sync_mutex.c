#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"
#include "test_support.h"

static volatile int g_mutex_shared_counter;
static gth_mutex_t g_test_mutex;

static void *mutex_increment_fn(void *arg)
{
    (void)arg;
    gth_mutex_lock(&g_test_mutex);
    int tmp = g_mutex_shared_counter;
    gth_thread_yield();
    g_mutex_shared_counter = tmp + 1;
    gth_mutex_unlock(&g_test_mutex);
    return NULL;
}

void test_mutex_mutual_exclusion_with_contention(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(42);
    gth_tid_t tids[5];
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_init(&g_test_mutex);
    assert_int_equal(rc, GTH_OK);

    g_mutex_shared_counter = 0;

    for (int i = 0; i < 5; ++i)
    {
        rc = gth_thread_create(&tids[i], NULL, mutex_increment_fn, NULL);
        assert_int_equal(rc, GTH_OK);
    }

    for (int i = 0; i < 5; ++i)
    {
        rc = gth_thread_join(tids[i], NULL);
        assert_int_equal(rc, GTH_OK);
    }

    assert_int_equal(g_mutex_shared_counter, 5);

    rc = gth_mutex_destroy(&g_test_mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_mutex_trylock_returns_busy_when_held(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(1);
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    gth_mutex_t m;
    rc = gth_mutex_init(&m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_lock(&m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_trylock(&m);
    assert_int_equal(rc, GTH_EBUSY);

    rc = gth_mutex_unlock(&m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_trylock(&m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_unlock(&m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_destroy(&m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

static volatile int g_mutex_block_flag;
static gth_mutex_t g_mutex_block_m;

static void *mutex_blocker_fn(void *arg)
{
    (void)arg;
    gth_mutex_lock(&g_mutex_block_m);
    g_mutex_block_flag = 1;
    gth_thread_yield();
    gth_mutex_unlock(&g_mutex_block_m);
    return NULL;
}

static void *mutex_waiter_fn(void *arg)
{
    (void)arg;
    gth_mutex_lock(&g_mutex_block_m);
    g_mutex_block_flag = 2;
    gth_mutex_unlock(&g_mutex_block_m);
    return NULL;
}

void test_mutex_blocking_on_contention(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(7);
    gth_tid_t blocker, waiter;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_init(&g_mutex_block_m);
    assert_int_equal(rc, GTH_OK);

    g_mutex_block_flag = 0;

    rc = gth_thread_create(&blocker, NULL, mutex_blocker_fn, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&waiter, NULL, mutex_waiter_fn, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(blocker, NULL);
    assert_int_equal(rc, GTH_OK);

    assert_int_equal(g_mutex_block_flag, 1);

    rc = gth_thread_join(waiter, NULL);
    assert_int_equal(rc, GTH_OK);

    assert_int_equal(g_mutex_block_flag, 2);

    rc = gth_mutex_destroy(&g_mutex_block_m);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_mutex_init_destroy_cycle(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(1);
    gth_mutex_t m;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    for (int i = 0; i < 10; ++i)
    {
        rc = gth_mutex_init(&m);
        assert_int_equal(rc, GTH_OK);

        rc = gth_mutex_lock(&m);
        assert_int_equal(rc, GTH_OK);

        rc = gth_mutex_unlock(&m);
        assert_int_equal(rc, GTH_OK);

        rc = gth_mutex_destroy(&m);
        assert_int_equal(rc, GTH_OK);
    }

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_mutex_rejects_null(void **state)
{
    (void)state;
    assert_int_equal(gth_mutex_init(NULL), GTH_EINVAL);
    assert_int_equal(gth_mutex_lock(NULL), GTH_EINVAL);
    assert_int_equal(gth_mutex_trylock(NULL), GTH_EINVAL);
    assert_int_equal(gth_mutex_unlock(NULL), GTH_EINVAL);
    assert_int_equal(gth_mutex_destroy(NULL), GTH_EINVAL);
}
