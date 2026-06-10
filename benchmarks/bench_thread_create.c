#define _POSIX_C_SOURCE 199309L

/*
 * bench_thread_create.c - Thread creation + join overhead benchmark
 *
 * Creates N threads in sequence. Each thread does minimal work (one
 * yield) then returns. Measures total time and computes average
 * nanoseconds per create+join pair.
 *
 * Build:
 *   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
 *   cmake --build build-release
 *   ./build-release/bench_thread_create
 */

#include <stdio.h>
#include <time.h>

#include <gthreads/gthreads.h>

#define NUM_THREADS 1000

static void *noop_fn(void *arg)
{
    (void)arg;
    gth_thread_yield();
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

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        gth_tid_t tid;
        if (gth_thread_create(&tid, NULL, noop_fn, NULL) != GTH_OK)
        {
            fprintf(stderr, "thread create failed at %d\n", i);
            gth_runtime_shutdown();
            return 1;
        }
        gth_thread_join(tid, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double total_ns = elapsed_ns(&t0, &t1);
    double ns_per_thread = total_ns / (double)NUM_THREADS;

    printf("--- Thread Create/Join Benchmark ---\n");
    printf("Threads created:    %d\n", NUM_THREADS);
    printf("Total time:         %.2f ms\n", total_ns / 1e6);
    printf("ns per create+join: %.1f\n", ns_per_thread);

    gth_runtime_shutdown();
    return 0;
}
