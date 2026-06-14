#define _POSIX_C_SOURCE 199309L

/*
 * bench_semaphore.c - Semaphore throughput benchmark (producer-consumer)
 *
 * Uses a producer-consumer pattern with N producers and 1 consumer.
 * Each producer posts M times to a shared semaphore; the consumer
 * waits the same total number of times. Measures throughput in
 * operations per second.
 *
 * Build:
 *   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
 *   cmake --build build-release
 *   ./build-release/bench_semaphore
 */

#include <stdio.h>
#include <time.h>

#include <gthreads/gthreads.h>

#define NUM_PRODUCERS 5
#define POSTS_PER_PRODUCER 1000

static gth_sem_t g_sem;

typedef struct
{
    int posts;
} producer_arg_t;

static void *producer_fn(void *arg)
{
    producer_arg_t *pa = (producer_arg_t *)arg;
    for (int i = 0; i < pa->posts; i++)
    {
        gth_sem_post(&g_sem);
    }
    return NULL;
}

static volatile int g_consumer_count;
static int g_total_expected;

static void *consumer_fn(void *arg)
{
    (void)arg;
    int consumed = 0;
    while (consumed < g_total_expected)
    {
        gth_sem_wait(&g_sem);
        consumed++;
    }
    g_consumer_count = consumed;
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

    if (gth_sem_init(&g_sem, 0) != GTH_OK)
    {
        fprintf(stderr, "sem init failed\n");
        gth_runtime_shutdown();
        return 1;
    }

    g_total_expected = NUM_PRODUCERS * POSTS_PER_PRODUCER;
    g_consumer_count = 0;

    /* Create consumer first so it is ready to receive posts */
    gth_tid_t consumer_tid;
    if (gth_thread_create(&consumer_tid, NULL, consumer_fn, NULL) != GTH_OK)
    {
        fprintf(stderr, "consumer create failed\n");
        gth_sem_destroy(&g_sem);
        gth_runtime_shutdown();
        return 1;
    }

    gth_tid_t producer_tids[NUM_PRODUCERS];
    producer_arg_t producer_args[NUM_PRODUCERS];

    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_args[i].posts = POSTS_PER_PRODUCER;
        if (gth_thread_create(&producer_tids[i], NULL, producer_fn, &producer_args[i]) != GTH_OK)
        {
            fprintf(stderr, "producer create failed at %d\n", i);
            gth_sem_destroy(&g_sem);
            gth_runtime_shutdown();
            return 1;
        }
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        gth_thread_join(producer_tids[i], NULL);
    }
    gth_thread_join(consumer_tid, NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double total_ns = elapsed_ns(&t0, &t1);
    double total_ops = (double)g_total_expected;
    double ops_per_sec = total_ops / (total_ns / 1e9);
    double ns_per_op = total_ns / total_ops;

    printf("--- Semaphore Throughput Benchmark ---\n");
    printf("Producers:          %d\n", NUM_PRODUCERS);
    printf("Posts/producer:     %d\n", POSTS_PER_PRODUCER);
    printf("Total operations:   %.0f\n", total_ops);
    printf("Consumed:           %d\n", g_consumer_count);
    printf("Total time:         %.2f ms\n", total_ns / 1e6);
    printf("ns per wait+post:   %.1f\n", ns_per_op);
    printf("Throughput:         %.0f ops/sec\n", ops_per_sec);

    gth_sem_destroy(&g_sem);
    gth_runtime_shutdown();
    return 0;
}
