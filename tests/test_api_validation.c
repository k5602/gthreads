#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"

static void *api_validation_noop_thread(void *arg)
{
    return arg;
}

static gth_runtime_config_t make_valid_config(uint32_t seed)
{
    gth_runtime_config_t cfg = {
        .stack_size_bytes = 64U * 1024U,
        .policy = GTH_SCHED_RR,
        .quantum_us = 1000U,
        .replay_seed = seed,
        .enable_deterministic_trace = 0,
        .enable_schedule_fuzzing = 0,
    };
    return cfg;
}

void test_api_validation(void **state)
{
    (void)state;

    gth_runtime_config_t valid_cfg = make_valid_config(1337U);

    assert_int_equal(gth_runtime_init(NULL), GTH_EINVAL);

    gth_runtime_config_t invalid_cfg = valid_cfg;
    invalid_cfg.stack_size_bytes = 0U;
    assert_int_equal(gth_runtime_init(&invalid_cfg), GTH_EINVAL);

    invalid_cfg = valid_cfg;
    invalid_cfg.stack_size_bytes = 8U * 1024U;
    assert_int_equal(gth_runtime_init(&invalid_cfg), GTH_EINVAL);

    invalid_cfg = valid_cfg;
    invalid_cfg.quantum_us = 0U;
    assert_int_equal(gth_runtime_init(&invalid_cfg), GTH_EINVAL);

    invalid_cfg = valid_cfg;
    invalid_cfg.policy = (gth_sched_policy_t)99;
    assert_int_equal(gth_runtime_init(&invalid_cfg), GTH_EINVAL);

    assert_int_equal(gth_runtime_init(&valid_cfg), GTH_OK);
    assert_int_equal(gth_runtime_init(&valid_cfg), GTH_ESTATE);

    assert_int_equal(gth_runtime_get_stats(NULL), GTH_EINVAL);

    gth_runtime_stats_t stats;
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_OK);
    assert_true(stats.context_switches == 0U);
    assert_true(stats.runnable_threads == 0U);
    assert_true(stats.blocked_threads == 0U);

    assert_int_equal(gth_thread_create(NULL, NULL, api_validation_noop_thread, NULL), GTH_EINVAL);

    gth_tid_t tid = 0;
    assert_int_equal(gth_thread_create(&tid, NULL, NULL, NULL), GTH_EINVAL);

    assert_int_equal(gth_thread_join(0U, NULL), GTH_EINVAL);
    assert_int_equal(gth_thread_join(99999U, NULL), GTH_ENOTFOUND);
    assert_int_equal(gth_thread_cancel(0U), GTH_EINVAL);
    assert_int_equal(gth_thread_cancel(99999U), GTH_ENOTFOUND);

    assert_int_equal(gth_trace_start(NULL), GTH_EINVAL);
    assert_int_equal(gth_trace_start(""), GTH_EINVAL);
    assert_int_equal(gth_replay_from(NULL), GTH_EINVAL);
    assert_int_equal(gth_replay_from(""), GTH_EINVAL);

    assert_int_equal(gth_runtime_shutdown(), GTH_OK);
    assert_int_equal(gth_runtime_shutdown(), GTH_ESTATE);
}
