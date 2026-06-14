#define _POSIX_C_SOURCE 199309L

/*
 * bench_context_switch.c - Context switch throughput benchmark
 *
 * Creates N threads, each yielding M times. Measures total wall-clock
 * time and computes average nanoseconds per context switch.
 *
 * The context switch count is taken from the runtime stats (which
 * includes all switches - from yield, create, join, and sync).
 * We also count user-visible yields to provide a tighter measurement
 * of cooperative switching cost.
 *
 * Build:
 *   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
 *   cmake --build build-release
 *   ./build-release/bench_context_switch
 */

#include <stdio.h>
#include <time.h>

#include <gthreads/gthreads.h>

#define NUM_THREADS 100
#define YIELD_COUNT 1000

typedef struct
{
    int iterations;
} yield_arg_t;

static void *yield_fn(void *arg)
{
    yield_arg_t *ya = (yield_arg_t *)arg;
    for (int i = 0; i < ya->iterations; i++)
    {
        gth_thread_yield();
    }
    return NULL;
}

static double elapsed_ns(struct timespec *start, struct timespec *end)
{
    double sec = (double)(end->tv_sec - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec);
    return sec * 1e9 + nsec;
}

int main(void)
{
    gth_runtime_config_t cfg = {
        .stack_size_bytes = 64U * 1024U,
        .policy = GTH_SCHED_RR,
        .quantum_us = 1000U,
        .replay_seed = 0,
        .enable_deterministic_trace = 0,
        .enable_schedule_fuzzing = 0,
    };

    if (gth_runtime_init(&cfg) != GTH_OK)
    {
        fprintf(stderr, "runtime init failed\n");
        return 1;
    }

    gth_tid_t tids[NUM_THREADS];
    yield_arg_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        args[i].iterations = YIELD_COUNT;
        if (gth_thread_create(&tids[i], NULL, yield_fn, &args[i]) != GTH_OK)
        {
            fprintf(stderr, "thread create failed at %d\n", i);
            gth_runtime_shutdown();
            return 1;
        }
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        gth_thread_join(tids[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double total_ns = elapsed_ns(&t0, &t1);
    double total_yields = (double)NUM_THREADS * YIELD_COUNT;
    double ns_per_yield = total_ns / total_yields;

    gth_runtime_stats_t stats;
    gth_runtime_get_stats(&stats);

    printf("--- Context Switch Benchmark ---\n");
    printf("Threads:            %d\n", NUM_THREADS);
    printf("Yields per thread:  %d\n", YIELD_COUNT);
    printf("Total yields:       %.0f\n", total_yields);
    printf("Total time:         %.2f ms\n", total_ns / 1e6);
    printf("ns per yield:       %.1f\n", ns_per_yield);
    printf("Total ctx switches: %lu\n", (unsigned long)stats.context_switches);
    printf("ns per ctx switch:  %.1f\n", total_ns / (double)stats.context_switches);

    gth_runtime_shutdown();
    return 0;
}
