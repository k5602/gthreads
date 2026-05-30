/*
 * basic_threads.c - Minimal gthreads example
 *
 * Demonstrates the core lifecycle:
 *   1. Initialize the runtime with default configuration
 *   2. Create threads that do work and yield
 *   3. Join all threads and collect return values
 *   4. Shut down the runtime
 *
 * Build:
 *   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
 *   cmake --build build
 *   ./build/basic_threads
 */

#include <stdint.h>
#include <stdio.h>

#include <gthreads/gthreads.h>

/* Shared state for demonstrating thread work */
typedef struct
{
    const char *name;
    int iterations;
    int result;
} worker_arg_t;

/*
 * Worker thread function. Each iteration does a small unit of work
 * then yields to let other threads run.
 */
static void *worker_fn(void *arg)
{
    worker_arg_t *w = (worker_arg_t *)arg;
    int sum = 0;

    printf("[%s] started (TID: %lu)\n", w->name, (unsigned long)gth_thread_self());

    for (int i = 0; i < w->iterations; i++)
    {
        sum += i + 1;
        printf("[%s] iteration %d/%d, partial sum = %d\n", w->name, i + 1, w->iterations, sum);

        /* Yield to let other threads make progress */
        gth_thread_yield();
    }

    w->result = sum;
    printf("[%s] finished, result = %d\n", w->name, sum);

    return &w->result;
}

int main(void)
{
    gth_status_t rc;

    /*
     * Step 1: Configure and initialize the runtime.
     * Default config: 64 KB stack per thread, round-robin scheduling,
     * 1 ms time quantum.
     */
    gth_runtime_config_t cfg = {
        .stack_size_bytes = 64U * 1024U,
        .policy = GTH_SCHED_RR,
        .quantum_us = 1000U,
        .replay_seed = 0,
        .enable_deterministic_trace = 0,
        .enable_schedule_fuzzing = 0,
    };

    printf("Initializing gthreads runtime...\n");
    rc = gth_runtime_init(&cfg);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Failed to init runtime: %d\n", rc);
        return 1;
    }

    /*
     * Step 2: Create worker threads.
     * Each thread gets a name and iteration count. The TID is filled in
     * by gth_thread_create after successful creation.
     */
    worker_arg_t workers[3] = {
        {.name = "worker-A", .iterations = 3, .result = 0},
        {.name = "worker-B", .iterations = 4, .result = 0},
        {.name = "worker-C", .iterations = 2, .result = 0},
    };

    gth_tid_t tids[3] = {0};

    for (int i = 0; i < 3; i++)
    {
        rc = gth_thread_create(&tids[i], NULL, worker_fn, &workers[i]);
        if (rc != GTH_OK)
        {
            fprintf(stderr, "Failed to create thread %d: %d\n", i, rc);
            gth_runtime_shutdown();
            return 1;
        }
        printf("Created thread %s with TID %lu\n", workers[i].name, (unsigned long)tids[i]);
    }

    /*
     * Step 3: Join all threads.
     * gth_thread_join blocks until the target thread finishes.
     * The return value from the thread function is retrieved via retval.
     */
    printf("\n--- Joining threads ---\n");
    for (int i = 0; i < 3; i++)
    {
        void *retval = NULL;
        rc = gth_thread_join(tids[i], &retval);
        if (rc != GTH_OK)
        {
            fprintf(stderr, "Failed to join thread %s: %d\n", workers[i].name, rc);
        }
        else
        {
            printf("Joined %s (result pointer: %p)\n", workers[i].name, retval);
        }
    }

    /*
     * Step 4: Print runtime statistics and shut down.
     */
    gth_runtime_stats_t stats;
    rc = gth_runtime_get_stats(&stats);
    if (rc == GTH_OK)
    {
        printf("\n--- Runtime stats ---\n");
        printf("Context switches: %lu\n", (unsigned long)stats.context_switches);
        printf("Runnable threads: %lu\n", (unsigned long)stats.runnable_threads);
        printf("Blocked threads:  %lu\n", (unsigned long)stats.blocked_threads);
    }

    printf("\nShutting down runtime...\n");
    rc = gth_runtime_shutdown();
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Failed to shut down: %d\n", rc);
        return 1;
    }

    printf("Done.\n");
    return 0;
}
