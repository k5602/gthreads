#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"

static void *noop_thread(void *arg)
{
    return arg;
}

void test_runtime_skeleton(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = {
        .stack_size_bytes = 64U * 1024U,
        .policy = GTH_SCHED_RR,
        .quantum_us = 1000U,
        .replay_seed = 7U,
        .enable_deterministic_trace = 1,
        .enable_schedule_fuzzing = 1,
    };

    assert_int_equal(gth_runtime_init(&cfg), GTH_OK);

    gth_tid_t tid = 0;
    assert_int_equal(gth_thread_create(&tid, NULL, noop_thread, NULL), GTH_OK);

    assert_int_equal(gth_thread_yield(), GTH_OK);

    assert_int_equal(gth_thread_cancel(tid), GTH_OK);
    assert_int_equal(gth_thread_join(tid, NULL), GTH_OK);

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

    assert_int_equal(gth_runtime_shutdown(), GTH_OK);
}
