#define _POSIX_C_SOURCE 199309L

/*
 * bench_mutex.c - Mutex lock/unlock throughput benchmark
 *
 * Creates N threads sharing a single mutex. Each thread performs K
 * lock+unlock cycles on a shared counter. Measures total time and
 * computes average nanoseconds per lock+unlock cycle.
 *
 * Build:
 *   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
 *   cmake --build build-release
 *   ./build-release/bench_mutex
 */

#include <stdio.h>
#include <time.h>

#include <gthreads/gthreads.h>

#define NUM_THREADS 10
#define LOCK_CYCLES 1000

static volatile int g_counter;
static gth_mutex_t g_mutex;

typedef struct
{
    int cycles;
} mutex_arg_t;

static void *mutex_worker(void *arg)
{
    mutex_arg_t *ma = (mutex_arg_t *)arg;
    for (int i = 0; i < ma->cycles; i++)
    {
        gth_mutex_lock(&g_mutex);
        g_counter++;
        gth_mutex_unlock(&g_mutex);
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

    if (gth_mutex_init(&g_mutex) != GTH_OK)
    {
        fprintf(stderr, "mutex init failed\n");
        gth_runtime_shutdown();
        return 1;
    }

    g_counter = 0;

    gth_tid_t tids[NUM_THREADS];
    mutex_arg_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        args[i].cycles = LOCK_CYCLES;
        if (gth_thread_create(&tids[i], NULL, mutex_worker, &args[i]) != GTH_OK)
        {
            fprintf(stderr, "thread create failed at %d\n", i);
            gth_mutex_destroy(&g_mutex);
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
    double total_cycles = (double)NUM_THREADS * LOCK_CYCLES;
    double ns_per_cycle = total_ns / total_cycles;

    printf("--- Mutex Contention Benchmark ---\n");
    printf("Threads:            %d\n", NUM_THREADS);
    printf("Lock cycles/thread: %d\n", LOCK_CYCLES);
    printf("Total cycles:       %.0f\n", total_cycles);
    printf("Counter:            %d (expected: %.0f)\n", g_counter, total_cycles);
    printf("Total time:         %.2f ms\n", total_ns / 1e6);
    printf("ns per lock+unlock: %.1f\n", ns_per_cycle);

    gth_mutex_destroy(&g_mutex);
    gth_runtime_shutdown();
    return 0;
}
