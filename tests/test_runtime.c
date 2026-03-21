#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"
#include "test_support.h"

static void *runtime_noop_thread(void *arg)
{
    return arg;
}

static void *runtime_cancelable_thread(void *arg)
{
    (void)arg;
    return (void *)0x1;
}

static void *runtime_increment_thread(void *arg)
{
    uint64_t *counter = (uint64_t *)arg;
    if (counter != NULL)
    {
        *counter += 1U;
    }
    return arg;
}

static void runtime_assert_started_rr(uint32_t seed)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(seed);
    assert_int_equal(gth_runtime_init(&cfg), GTH_OK);
}

static void runtime_assert_shutdown_ok(void)
{
    assert_int_equal(gth_runtime_shutdown(), GTH_OK);
}

static void runtime_assert_shutdown_clears_stats(void)
{
    gth_runtime_stats_t stats;
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_ESTATE);
}

void test_runtime_executes_and_joins_threads(void **state)
{
    (void)state;

    runtime_assert_started_rr(7U);

    int retval_marker = 42;
    void *retval = NULL;

    gth_tid_t tid = 0;
    assert_int_equal(gth_thread_create(&tid, NULL, runtime_noop_thread, &retval_marker), GTH_OK);

    assert_int_equal(gth_thread_yield(), GTH_OK);
    assert_int_equal(gth_thread_join(tid, &retval), GTH_OK);
    assert_ptr_equal(retval, &retval_marker);

    gth_tid_t canceled_tid = 0;
    assert_int_equal(gth_thread_create(&canceled_tid, NULL, runtime_cancelable_thread, NULL),
                     GTH_OK);
    assert_int_equal(gth_thread_cancel(canceled_tid), GTH_OK);

    retval = (void *)0x2;
    assert_int_equal(gth_thread_join(canceled_tid, &retval), GTH_OK);
    assert_null(retval);

    gth_mutex_t m;
    assert_int_equal(gth_mutex_init(&m), GTH_OK);
    assert_int_equal(gth_mutex_lock(&m), GTH_OK);
    assert_int_equal(gth_mutex_unlock(&m), GTH_OK);
    assert_int_equal(gth_mutex_destroy(&m), GTH_OK);

    gth_sem_t s;
    assert_int_equal(gth_sem_init(&s, 1U), GTH_OK);
    assert_int_equal(gth_sem_wait(&s), GTH_OK);
    assert_int_equal(gth_sem_post(&s), GTH_OK);
    assert_int_equal(gth_sem_destroy(&s), GTH_OK);

    gth_runtime_stats_t stats;
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_OK);
    assert_true(stats.context_switches > 0U);
    assert_int_equal(stats.runnable_threads, 0U);
    assert_int_equal(stats.blocked_threads, 0U);

    runtime_assert_shutdown_ok();
    runtime_assert_shutdown_clears_stats();
}

void test_runtime_join_drives_ready_thread(void **state)
{
    (void)state;

    runtime_assert_started_rr(11U);

    int retval_marker = 99;
    void *retval = NULL;
    gth_tid_t tid = 0;

    assert_int_equal(gth_thread_create(&tid, NULL, runtime_noop_thread, &retval_marker), GTH_OK);
    assert_int_equal(gth_thread_join(tid, &retval), GTH_OK);
    assert_ptr_equal(retval, &retval_marker);

    gth_runtime_stats_t stats;
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_OK);
    assert_true(stats.context_switches > 0U);

    runtime_assert_shutdown_ok();
}

void test_m2_rr_fairness_regression(void **state)
{
    (void)state;

    runtime_assert_started_rr(21U);

    uint64_t counter_a = 0U;
    uint64_t counter_b = 0U;
    uint64_t counter_c = 0U;

    gth_tid_t tid_a = 0;
    gth_tid_t tid_b = 0;
    gth_tid_t tid_c = 0;

    assert_int_equal(gth_thread_create(&tid_a, NULL, runtime_increment_thread, &counter_a), GTH_OK);
    assert_int_equal(gth_thread_create(&tid_b, NULL, runtime_increment_thread, &counter_b), GTH_OK);
    assert_int_equal(gth_thread_create(&tid_c, NULL, runtime_increment_thread, &counter_c), GTH_OK);

    assert_int_equal(gth_thread_yield(), GTH_OK);
    assert_int_equal(gth_thread_yield(), GTH_OK);
    assert_int_equal(gth_thread_yield(), GTH_OK);

    assert_int_equal(counter_a, 1U);
    assert_int_equal(counter_b, 1U);
    assert_int_equal(counter_c, 1U);

    assert_int_equal(gth_thread_join(tid_a, NULL), GTH_OK);
    assert_int_equal(gth_thread_join(tid_b, NULL), GTH_OK);
    assert_int_equal(gth_thread_join(tid_c, NULL), GTH_OK);

    gth_runtime_stats_t stats;
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_OK);
    assert_true(stats.context_switches >= 3U);
    assert_int_equal(stats.runnable_threads, 0U);
    assert_int_equal(stats.blocked_threads, 0U);

    runtime_assert_shutdown_ok();
}

void test_m2_lifecycle_stress_regression(void **state)
{
    (void)state;

    runtime_assert_started_rr(34U);

    enum
    {
        thread_count = 32
    };

    gth_tid_t tids[thread_count];
    uint64_t counters[thread_count];

    for (size_t i = 0; i < thread_count; ++i)
    {
        tids[i] = 0U;
        counters[i] = 0U;
        assert_int_equal(gth_thread_create(&tids[i], NULL, runtime_increment_thread, &counters[i]),
                         GTH_OK);
    }

    for (size_t i = 0; i < thread_count; ++i)
    {
        assert_int_equal(gth_thread_yield(), GTH_OK);
    }

    for (size_t i = 0; i < thread_count; ++i)
    {
        assert_int_equal(counters[i], 1U);
        assert_int_equal(gth_thread_join(tids[i], NULL), GTH_OK);
    }

    gth_runtime_stats_t stats;
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_OK);
    assert_true(stats.context_switches >= thread_count);
    assert_int_equal(stats.runnable_threads, 0U);
    assert_int_equal(stats.blocked_threads, 0U);

    runtime_assert_shutdown_ok();
}
