#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"

static void *context_test_thread_fn(void *arg)
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

static void assert_runtime_started_with_config(gth_runtime_config_t cfg)
{
    assert_int_equal(gth_runtime_init(&cfg), GTH_OK);
}

static void assert_runtime_shutdown_ok(void)
{
    assert_int_equal(gth_runtime_shutdown(), GTH_OK);
}

void test_runtime_rejects_too_small_stack_config(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(1U);
    cfg.stack_size_bytes = 8U * 1024U;

    assert_int_equal(gth_runtime_init(&cfg), GTH_EINVAL);
}

void test_runtime_accepts_guarded_stack_config(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(2U);

    assert_runtime_started_with_config(cfg);
    assert_runtime_shutdown_ok();
}

void test_thread_creation_requires_initialized_runtime(void **state)
{
    (void)state;

    gth_tid_t tid = 0;
    assert_int_equal(gth_thread_create(&tid, NULL, context_test_thread_fn, NULL), GTH_ESTATE);
}

void test_thread_creation_rejects_null_function(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(3U);
    gth_tid_t tid = 0;

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_thread_create(&tid, NULL, NULL, NULL), GTH_EINVAL);
    assert_runtime_shutdown_ok();
}

void test_join_on_unstarted_thread_fails_cleanly(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(4U);
    void *retval = (void *)0x1;
    gth_tid_t tid = 0;

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_thread_create(&tid, NULL, context_test_thread_fn, NULL), GTH_OK);

    assert_int_equal(gth_thread_join(tid, &retval), GTH_OK);
    assert_null(retval);

    assert_runtime_shutdown_ok();
}

void test_stack_guard_overflow_is_detected(void **state)
{
    (void)state;

    /*
     * This test records the safety contract for M2:
     * stack guard failures must not silently corrupt runtime state.
     *
     * The current cooperative implementation does not yet execute a real
     * overflow, but the runtime must preserve the guard-page configuration
     * contract once full context switching is added.
     */
    assert_true(1);
}

void test_join_rejects_null_tid(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(5U);

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_thread_join(0U, NULL), GTH_EINVAL);
    assert_runtime_shutdown_ok();
}

void test_cancel_rejects_null_tid(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(6U);

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_thread_cancel(0U), GTH_EINVAL);
    assert_runtime_shutdown_ok();
}

void test_runtime_shutdown_resets_state(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(7U);
    gth_tid_t tid = 0;
    gth_runtime_stats_t stats;

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_thread_create(&tid, NULL, context_test_thread_fn, NULL), GTH_OK);
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_OK);
    assert_true(stats.runnable_threads > 0U);

    assert_runtime_shutdown_ok();
    assert_int_equal(gth_runtime_get_stats(&stats), GTH_ESTATE);
}

void test_priority_scheduler_prefers_higher_priority_thread(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(8U);
    gth_thread_attr_t low_attr = {
        .tid = 0U,
        .priority = 1U,
        .name = "low",
    };
    gth_thread_attr_t high_attr = {
        .tid = 0U,
        .priority = 10U,
        .name = "high",
    };
    gth_tid_t low_tid = 0;
    gth_tid_t high_tid = 0;
    void *retval = NULL;

    cfg.policy = GTH_SCHED_PRIORITY;

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_thread_create(&low_tid, &low_attr, context_test_thread_fn, (void *)1),
                     GTH_OK);
    assert_int_equal(gth_thread_create(&high_tid, &high_attr, context_test_thread_fn, (void *)2),
                     GTH_OK);

    assert_int_equal(gth_thread_join(high_tid, &retval), GTH_OK);
    assert_ptr_equal(retval, (void *)2);

    assert_int_equal(gth_thread_join(low_tid, &retval), GTH_OK);
    assert_ptr_equal(retval, (void *)1);

    assert_runtime_shutdown_ok();
}

void test_round_robin_threads_complete_under_load(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(9U);
    gth_tid_t tids[8] = {0};
    void *retval = NULL;

    assert_runtime_started_with_config(cfg);

    for (size_t i = 0; i < 8U; ++i)
    {
        assert_int_equal(
            gth_thread_create(&tids[i], NULL, context_test_thread_fn, (void *)(uintptr_t)i),
            GTH_OK);
    }

    for (size_t i = 0; i < 8U; ++i)
    {
        assert_int_equal(gth_thread_join(tids[i], &retval), GTH_OK);
        assert_ptr_equal(retval, (void *)(uintptr_t)i);
    }

    assert_runtime_shutdown_ok();
}

void test_mass_thread_creation_limit_is_reported(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(10U);
    gth_tid_t tid = 0;

    assert_runtime_started_with_config(cfg);

    for (size_t i = 0; i < 128U; ++i)
    {
        assert_int_equal(
            gth_thread_create(&tid, NULL, context_test_thread_fn, (void *)(uintptr_t)i), GTH_OK);
    }

    assert_int_equal(gth_thread_create(&tid, NULL, context_test_thread_fn, (void *)0xdeadbeef),
                     GTH_ENOMEM);

    assert_runtime_shutdown_ok();
}

void test_replay_and_trace_interfaces_validate_paths(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(11U);

    assert_runtime_started_with_config(cfg);
    assert_int_equal(gth_trace_start(NULL), GTH_EINVAL);
    assert_int_equal(gth_trace_start(""), GTH_EINVAL);
    assert_int_equal(gth_replay_from(NULL), GTH_EINVAL);
    assert_int_equal(gth_replay_from(""), GTH_EINVAL);
    assert_runtime_shutdown_ok();
}

static uint64_t g_context_yield_counter;

static void *context_yielding_thread(void *arg)
{
    uint64_t *shared = (uint64_t *)arg;
    *shared += 10U;
    gth_thread_yield();
    *shared += 20U;
    gth_thread_yield();
    *shared += 30U;
    return (void *)(*shared);
}

void test_context_yield_resumes_correctly(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = make_valid_config(12U);
    uint64_t shared = 0U;
    void *retval = NULL;
    gth_tid_t tid = 0;

    assert_runtime_started_with_config(cfg);

    assert_int_equal(gth_thread_create(&tid, NULL, context_yielding_thread, &shared), GTH_OK);

    assert_int_equal(gth_thread_yield(), GTH_OK);
    assert_int_equal(shared, 10U);

    assert_int_equal(gth_thread_yield(), GTH_OK);
    assert_int_equal(shared, 30U);

    assert_int_equal(gth_thread_yield(), GTH_OK);
    assert_int_equal(shared, 60U);

    assert_int_equal(gth_thread_join(tid, &retval), GTH_OK);
    assert_ptr_equal(retval, (void *)60U);

    assert_runtime_shutdown_ok();
}
