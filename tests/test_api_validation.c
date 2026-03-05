#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"

static void *noop_thread(void *arg)
{
    return arg;
}

void test_api_validation(void **state)
{
    (void)state;

    gth_runtime_config_t cfg = {
        .stack_size_bytes = 64U * 1024U,
        .policy = GTH_SCHED_RR,
        .quantum_us = 1000U,
        .replay_seed = 1337U,
        .enable_deterministic_trace = 0,
        .enable_schedule_fuzzing = 0,
    };

    assert_int_equal(gth_runtime_init(NULL), GTH_EINVAL);
    assert_int_equal(gth_runtime_init(&cfg), GTH_OK);

    assert_int_equal(gth_thread_create(NULL, NULL, noop_thread, NULL), GTH_EINVAL);

    gth_tid_t tid = 0;
    assert_int_equal(gth_thread_create(&tid, NULL, NULL, NULL), GTH_EINVAL);

    assert_int_equal(gth_thread_join(99999U, NULL), GTH_ENOTFOUND);

    assert_int_equal(gth_trace_start(NULL), GTH_EINVAL);

    assert_int_equal(gth_runtime_shutdown(), GTH_OK);
}
