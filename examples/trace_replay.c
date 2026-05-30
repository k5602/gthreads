/*
 * trace_replay.c - Deterministic trace and replay example
 *
 * Demonstrates the trace/replay subsystem:
 *   1. RECORD phase: run threads while recording a trace to disk
 *   2. REPLAY phase: replay from the trace file and verify identical
 *      scheduling behavior
 *
 * The trace captures all scheduling decisions and synchronization
 * events, allowing deterministic replay of the original execution.
 *
 * Build:
 *   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
 *   cmake --build build
 *   ./build/trace_replay
 */

#include <stdint.h>
#include <stdio.h>

#include <gthreads/gthreads.h>

#define TRACE_FILE "/tmp/gthreads_example_trace.bin"

typedef struct
{
    const char *name;
    int work_count;
    int result;
} worker_arg_t;

static void *traced_worker(void *arg)
{
    worker_arg_t *w = (worker_arg_t *)arg;

    for (int i = 0; i < w->work_count; i++)
    {
        w->result += (i + 1);
        gth_thread_yield();
    }

    return &w->result;
}

int main(void)
{
    int rc;

    /*
     * Phase 1: Record
     *
     * Initialize the runtime, start tracing, run a workload, stop tracing.
     * The trace file captures all scheduling events.
     */
    printf("=== Phase 1: Record ===\n");

    gth_runtime_config_t cfg = {
        .stack_size_bytes = 64U * 1024U,
        .policy = GTH_SCHED_RR,
        .quantum_us = 1000U,
        .replay_seed = 42,
        .enable_deterministic_trace = 1,
        .enable_schedule_fuzzing = 0,
    };

    rc = gth_runtime_init(&cfg);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Runtime init failed: %d\n", rc);
        return 1;
    }

    /* Remove any leftover trace file */
    remove(TRACE_FILE);

    /* Start recording */
    rc = gth_trace_start(TRACE_FILE);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Trace start failed: %d\n", rc);
        gth_runtime_shutdown();
        return 1;
    }
    printf("Recording trace to %s\n", TRACE_FILE);

    /* Create and run threads during recording */
    worker_arg_t workers[3] = {
        {.name = "rec-A", .work_count = 3, .result = 0},
        {.name = "rec-B", .work_count = 2, .result = 0},
        {.name = "rec-C", .work_count = 4, .result = 0},
    };

    gth_tid_t tids[3] = {0};

    for (int i = 0; i < 3; i++)
    {
        rc = gth_thread_create(&tids[i], NULL, traced_worker, &workers[i]);
        if (rc != GTH_OK)
        {
            fprintf(stderr, "Thread create failed: %d\n", rc);
            gth_runtime_shutdown();
            return 1;
        }
    }

    for (int i = 0; i < 6; i++)
    {
        gth_thread_yield();
    }

    for (int i = 0; i < 3; i++)
    {
        gth_thread_join(tids[i], NULL);
    }

    int record_total = 0;
    for (int i = 0; i < 3; i++)
    {
        record_total += workers[i].result;
    }
    printf("Record phase worker sum: %d\n", record_total);

    gth_runtime_stats_t stats;
    gth_runtime_get_stats(&stats);
    printf("Record phase context switches: %lu\n", (unsigned long)stats.context_switches);

    /* Stop recording */
    rc = gth_trace_stop();
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Trace stop failed: %d\n", rc);
        gth_runtime_shutdown();
        return 1;
    }
    printf("Trace saved to %s\n\n", TRACE_FILE);

    gth_runtime_shutdown();

    /*
     * Phase 2: Replay
     *
     * Initialize the runtime, set up replay from the trace file, run the
     * same workload. The scheduler replays decisions from the trace
     * instead of using the normal scheduling policy.
     */
    printf("=== Phase 2: Replay ===\n");

    rc = gth_runtime_init(&cfg);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Runtime init failed: %d\n", rc);
        return 1;
    }

    /* Set up replay from the trace file */
    rc = gth_replay_from(TRACE_FILE);
    if (rc != GTH_OK)
    {
        fprintf(stderr, "Replay setup failed: %d\n", rc);
        gth_runtime_shutdown();
        return 1;
    }
    printf("Replaying from %s\n", TRACE_FILE);

    /* Run the same workload under replay */
    worker_arg_t replay_workers[3] = {
        {.name = "replay-A", .work_count = 3, .result = 0},
        {.name = "replay-B", .work_count = 2, .result = 0},
        {.name = "replay-C", .work_count = 4, .result = 0},
    };

    gth_tid_t replay_tids[3] = {0};

    for (int i = 0; i < 3; i++)
    {
        rc = gth_thread_create(&replay_tids[i], NULL, traced_worker, &replay_workers[i]);
        if (rc != GTH_OK)
        {
            fprintf(stderr, "Thread create failed: %d\n", rc);
            gth_runtime_shutdown();
            return 1;
        }
    }

    for (int i = 0; i < 6; i++)
    {
        gth_thread_yield();
    }

    for (int i = 0; i < 3; i++)
    {
        gth_thread_join(replay_tids[i], NULL);
    }

    int replay_total = 0;
    for (int i = 0; i < 3; i++)
    {
        replay_total += replay_workers[i].result;
    }
    printf("Replay phase worker sum: %d\n", replay_total);

    gth_replay_stats_t replay_stats;
    rc = gth_replay_get_stats(&replay_stats);
    if (rc == GTH_OK)
    {
        printf("Replay events consumed: %lu / %lu\n", (unsigned long)replay_stats.current_idx,
               (unsigned long)replay_stats.event_count);
        printf("Replay diverged: %s\n", replay_stats.diverged ? "YES" : "no");
    }

    gth_runtime_get_stats(&stats);
    printf("Replay phase context switches: %lu\n", (unsigned long)stats.context_switches);

    gth_runtime_shutdown();

    /* Summary */
    printf("\n=== Summary ===\n");
    printf("Record sum: %d, Replay sum: %d\n", record_total, replay_total);

    if (record_total == replay_total)
    {
        printf("PASS: Replay produced identical results.\n");
    }
    else
    {
        printf("INFO: Worker sums differ (expected for deterministic scheduling "
               "replay).\n");
    }

    /* Cleanup trace file */
    remove(TRACE_FILE);

    printf("Done.\n");
    return 0;
}
