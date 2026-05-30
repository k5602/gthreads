/*
 * sync_demo.c - Mutex synchronization example
 *
 * Demonstrates mutual exclusion with a shared counter:
 *   1. Initialize the runtime
 *   2. Create a mutex protecting a shared counter
 *   3. Spawn 2 threads that compete for the mutex
 *   4. Each thread locks, increments the counter, yields (while
 *      holding the lock), then unlocks
 *   5. Verify the counter is correct (no races)
 *   6. Shut down
 *
 * The yield-while-locked pattern forces the scheduler to attempt
 * switching to the other thread, proving that the mutex blocks it.
 *
 * Build:
 *   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
 *   cmake --build build
 *   ./build/sync_demo
 */

#include <stdint.h>
#include <stdio.h>

#include <gthreads/gthreads.h>

#define INCREMENTS_PER_THREAD 5

static int g_counter = 0;
static gth_mutex_t g_mutex;

typedef struct
{
    const char *name;
    int increments;
} incrementor_arg_t;

/*
 * Thread that acquires the mutex, does work, and releases it.
 * The yield-while-locked ensures the other thread gets scheduled
 * and attempts to acquire the same mutex (proving it blocks).
 */
static void *incrementor_fn(void *arg)
{
    incrementor_arg_t *ia = (incrementor_arg_t *)arg;

    printf("[%s] started (TID: %lu)\n", ia->name, (unsigned long)gth_thread_self());

    for (int i = 0; i < ia->increments; i++)
    {
        /* Acquire the mutex - will block if held by the other thread */
        gth_mutex_lock(&g_mutex);

        int before = g_counter;
        g_counter++;
        printf("[%s] locked, counter: %d -> %d\n", ia->name, before, g_counter);

        /* Yield while holding the lock. The other thread will attempt
         * to lock the mutex and block until we unlock. */
        gth_thread_yield();

        gth_mutex_unlock(&g_mutex);

        printf("[%s] unlocked\n", ia->name);
    }

    printf("[%s] finished\n", ia->name);
    return NULL;
}

int main(void)
{
    gth_status_t rc;

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

    /* Initialize the mutex */
    rc = gth_mutex_init(&g_mutex);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Failed to init mutex: %d\n", rc);
        gth_runtime_shutdown();
        return 1;
    }

    g_counter = 0;

    /*
     * Create 2 threads competing for the same mutex.
     * Each performs INCREMENTS_PER_THREAD locked increments.
     * Total expected counter value: 2 * INCREMENTS_PER_THREAD.
     */
    incrementor_arg_t args[2] = {
        {.name = "thread-A", .increments = INCREMENTS_PER_THREAD},
        {.name = "thread-B", .increments = INCREMENTS_PER_THREAD},
    };

    gth_tid_t tids[2] = {0};

    for (int i = 0; i < 2; i++)
    {
        rc = gth_thread_create(&tids[i], NULL, incrementor_fn, &args[i]);
        if (rc != GTH_OK)
        {
            fprintf(stderr, "Failed to create thread %d: %d\n", i, rc);
            gth_mutex_destroy(&g_mutex);
            gth_runtime_shutdown();
            return 1;
        }
    }

    /* Join both threads */
    for (int i = 0; i < 2; i++)
    {
        rc = gth_thread_join(tids[i], NULL);
        if (rc != GTH_OK)
        {
            fprintf(stderr, "Failed to join thread %d: %d\n", i, rc);
        }
    }

    /* Verify mutual exclusion was maintained */
    int expected = 2 * INCREMENTS_PER_THREAD;
    printf("\n--- Result ---\n");
    printf("Counter: %d (expected: %d)\n", g_counter, expected);

    if (g_counter == expected)
    {
        printf("PASS: Mutual exclusion preserved.\n");
    }
    else
    {
        printf("FAIL: Race condition detected!\n");
    }

    /* Cleanup */
    rc = gth_mutex_destroy(&g_mutex);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Failed to destroy mutex: %d\n", rc);
    }

    printf("\nShutting down runtime...\n");
    gth_runtime_shutdown();

    printf("Done.\n");
    return (g_counter == expected) ? 0 : 1;
}
