#ifndef GTHREADS_TEST_SUPPORT_H
#define GTHREADS_TEST_SUPPORT_H

#include <stdint.h>

#include "gthreads/gthreads.h"

#define GTH_TEST_MASS_THREAD_LIMIT 128U
#define GTH_TEST_DEFAULT_STACK_SIZE (64U * 1024U)
#define GTH_TEST_DEFAULT_QUANTUM_US 1000U

static inline gth_runtime_config_t gth_test_make_config(uint32_t seed, gth_sched_policy_t policy)
{
    gth_runtime_config_t cfg = {
        .stack_size_bytes = GTH_TEST_DEFAULT_STACK_SIZE,
        .policy = policy,
        .quantum_us = GTH_TEST_DEFAULT_QUANTUM_US,
        .replay_seed = seed,
        .enable_deterministic_trace = 0,
        .enable_schedule_fuzzing = 0,
    };
    return cfg;
}

static inline gth_runtime_config_t gth_test_make_rr_config(uint32_t seed)
{
    return gth_test_make_config(seed, GTH_SCHED_RR);
}

static inline gth_runtime_config_t gth_test_make_priority_config(uint32_t seed)
{
    return gth_test_make_config(seed, GTH_SCHED_PRIORITY);
}

#endif
