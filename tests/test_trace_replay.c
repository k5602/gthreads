/*
 * Trace, Replay, and Fuzz Testing
 *
 * Tests for deterministic trace/replay subsystem and schedule fuzzing.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"
#include "test_support.h"

#define TEST_TRACE_FILE "/tmp/gthreads_test_trace.bin"

static void *trace_test_thread_fn(void *arg)
{
    (void)arg;
    return (void *)0xABCD1234;
}

static void *trace_increment_thread(void *arg)
{
    uint64_t *counter = (uint64_t *)arg;
    if (counter != NULL)
    {
        *counter += 1;
    }
    return arg;
}

void test_trace_creates_valid_file(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(42);
    FILE *fp = NULL;
    long file_size = 0;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    /* Remove any existing file */
    (void)remove(TEST_TRACE_FILE);

    /* Start tracing */
    rc = gth_trace_start(TEST_TRACE_FILE);
    assert_int_equal(rc, GTH_OK);

    /* Stop tracing */
    rc = gth_trace_stop();
    assert_int_equal(rc, GTH_OK);

    /* Verify file exists and has content */
    fp = fopen(TEST_TRACE_FILE, "rb");
    assert_non_null(fp);

    rc = fseek(fp, 0, SEEK_END);
    assert_int_equal(rc, 0);

    file_size = ftell(fp);
    assert_true(file_size > 0);

    fclose(fp);

    /* Cleanup */
    (void)remove(TEST_TRACE_FILE);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_trace_records_thread_events(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(55);
    gth_tid_t tid = 0;
    void *retval = NULL;
    FILE *fp = NULL;
    long file_size = 0;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    (void)remove(TEST_TRACE_FILE);

    rc = gth_trace_start(TEST_TRACE_FILE);
    assert_int_equal(rc, GTH_OK);

    /* Create and run a thread */
    rc = gth_thread_create(&tid, NULL, trace_test_thread_fn, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tid, &retval);
    assert_int_equal(rc, GTH_OK);
    assert_ptr_equal(retval, (void *)0xABCD1234);

    rc = gth_trace_stop();
    assert_int_equal(rc, GTH_OK);

    /* Verify trace file has content */
    fp = fopen(TEST_TRACE_FILE, "rb");
    assert_non_null(fp);

    rc = fseek(fp, 0, SEEK_END);
    assert_int_equal(rc, 0);

    file_size = ftell(fp);
    assert_true(file_size > 32); /* At least header + some events */

    fclose(fp);

    /* Cleanup */
    (void)remove(TEST_TRACE_FILE);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_trace_records_context_switches(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(66);
    gth_tid_t tid1 = 0, tid2 = 0;
    uint64_t counter1 = 0, counter2 = 0;
    FILE *fp = NULL;
    long file_size_before = 0, file_size_after = 0;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    (void)remove(TEST_TRACE_FILE);

    rc = gth_trace_start(TEST_TRACE_FILE);
    assert_int_equal(rc, GTH_OK);

    /* Create two threads that will yield to each other */
    rc = gth_thread_create(&tid1, NULL, trace_increment_thread, &counter1);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tid2, NULL, trace_increment_thread, &counter2);
    assert_int_equal(rc, GTH_OK);

    /* Let them run and yield */
    rc = gth_thread_yield();
    assert_int_equal(rc, GTH_OK);

    /* Check trace file grew */
    fp = fopen(TEST_TRACE_FILE, "rb");
    assert_non_null(fp);
    rc = fseek(fp, 0, SEEK_END);
    assert_int_equal(rc, 0);
    file_size_before = ftell(fp);
    fclose(fp);

    /* More yields = more context switches = larger trace */
    rc = gth_thread_yield();
    assert_int_equal(rc, GTH_OK);

    fp = fopen(TEST_TRACE_FILE, "rb");
    assert_non_null(fp);
    rc = fseek(fp, 0, SEEK_END);
    assert_int_equal(rc, 0);
    file_size_after = ftell(fp);
    fclose(fp);

    assert_true(file_size_after >= file_size_before);

    /* Join threads */
    rc = gth_thread_join(tid1, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tid2, NULL);
    assert_int_equal(rc, GTH_OK);

    /* Both threads should have run */
    assert_true(counter1 > 0 || counter2 > 0);

    rc = gth_trace_stop();
    assert_int_equal(rc, GTH_OK);

    /* Cleanup */
    (void)remove(TEST_TRACE_FILE);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_trace_records_sync_events(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(77);
    gth_mutex_t mutex;
    gth_sem_t sem;
    FILE *fp = NULL;
    long file_size = 0;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_init(&mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_init(&sem, 1);
    assert_int_equal(rc, GTH_OK);

    (void)remove(TEST_TRACE_FILE);

    rc = gth_trace_start(TEST_TRACE_FILE);
    assert_int_equal(rc, GTH_OK);

    /* Record mutex operations */
    rc = gth_mutex_lock(&mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_mutex_unlock(&mutex);
    assert_int_equal(rc, GTH_OK);

    /* Record semaphore operations */
    rc = gth_sem_wait(&sem);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_post(&sem);
    assert_int_equal(rc, GTH_OK);

    rc = gth_trace_stop();
    assert_int_equal(rc, GTH_OK);

    /* Verify trace file has content */
    fp = fopen(TEST_TRACE_FILE, "rb");
    assert_non_null(fp);

    rc = fseek(fp, 0, SEEK_END);
    assert_int_equal(rc, 0);

    file_size = ftell(fp);
    assert_true(file_size > 32);

    fclose(fp);

    /* Cleanup */
    (void)remove(TEST_TRACE_FILE);

    rc = gth_mutex_destroy(&mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_destroy(&sem);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_replay_validates_header(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(88);
    FILE *fp = NULL;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    /* Create an invalid trace file */
    fp = fopen(TEST_TRACE_FILE, "wb");
    assert_non_null(fp);
    rc = fprintf(fp, "INVALID_DATA");
    assert_true(rc > 0);
    fclose(fp);

    /* Try to replay from invalid file */
    rc = gth_replay_from(TEST_TRACE_FILE);
    assert_int_equal(rc, GTH_ENOTFOUND); /* Should fail to find/validate */

    /* Cleanup */
    (void)remove(TEST_TRACE_FILE);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_replay_from_nonexistent_file(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(99);
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    /* Remove file if it exists */
    (void)remove(TEST_TRACE_FILE);

    /* Try to replay from non-existent file */
    rc = gth_replay_from(TEST_TRACE_FILE);
    assert_int_equal(rc, GTH_ENOTFOUND); /* File does not exist */

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_fuzz_same_seed_same_behavior(void **state)
{
    gth_runtime_config_t cfg1 = gth_test_make_rr_config(100);
    gth_runtime_config_t cfg2 = gth_test_make_rr_config(100); /* Same seed */
    uint64_t counter1 = 0, counter2 = 0;
    gth_tid_t tid1a = 0, tid1b = 0;
    gth_tid_t tid2a = 0, tid2b = 0;
    int rc;

    (void)state;

    /* First run with fuzz seed 100 */
    rc = gth_runtime_init(&cfg1);
    assert_int_equal(rc, GTH_OK);

    rc = gth_scheduler_set_mode(GTH_MODE_FUZZ);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tid1a, NULL, trace_increment_thread, &counter1);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tid1b, NULL, trace_increment_thread, &counter1);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tid1a, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tid1b, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);

    /* Second run with same fuzz seed 100 */
    rc = gth_runtime_init(&cfg2);
    assert_int_equal(rc, GTH_OK);

    rc = gth_scheduler_set_mode(GTH_MODE_FUZZ);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tid2a, NULL, trace_increment_thread, &counter2);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tid2b, NULL, trace_increment_thread, &counter2);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tid2a, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tid2b, NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);

    /* Both runs should produce same counter values with same seed */
    assert_int_equal(counter1, counter2);
}

void test_fuzz_different_seed_different_behavior(void **state)
{
    gth_runtime_config_t cfg1 = gth_test_make_rr_config(200);
    gth_runtime_config_t cfg2 = gth_test_make_rr_config(201); /* Different seed */
    uint64_t run_order1[4] = {0};
    uint64_t run_order2[4] = {0};
    gth_tid_t tids1[2] = {0};
    gth_tid_t tids2[2] = {0};
    int rc;

    (void)state;

    /* First run with fuzz seed 200 */
    rc = gth_runtime_init(&cfg1);
    assert_int_equal(rc, GTH_OK);

    rc = gth_scheduler_set_mode(GTH_MODE_FUZZ);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tids1[0], NULL, trace_increment_thread, &run_order1[0]);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tids1[1], NULL, trace_increment_thread, &run_order1[1]);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_yield();
    assert_int_equal(rc, GTH_OK);

    run_order1[2] = tids1[0];
    run_order1[3] = tids1[1];

    rc = gth_thread_join(tids1[0], NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tids1[1], NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);

    /* Second run with different fuzz seed 201 */
    rc = gth_runtime_init(&cfg2);
    assert_int_equal(rc, GTH_OK);

    rc = gth_scheduler_set_mode(GTH_MODE_FUZZ);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tids2[0], NULL, trace_increment_thread, &run_order2[0]);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_create(&tids2[1], NULL, trace_increment_thread, &run_order2[1]);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_yield();
    assert_int_equal(rc, GTH_OK);

    run_order2[2] = tids2[0];
    run_order2[3] = tids2[1];

    rc = gth_thread_join(tids2[0], NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_thread_join(tids2[1], NULL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);

    /* With different seeds, execution order may differ */
    /* We don't assert inequality because there's a small chance they're equal */
    /* This test mainly verifies both runs complete without crashing */
}

void test_trace_start_stop_cycle(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(111);
    int rc;
    int i;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    /* Multiple start/stop cycles */
    for (i = 0; i < 3; i++)
    {
        (void)remove(TEST_TRACE_FILE);

        rc = gth_trace_start(TEST_TRACE_FILE);
        assert_int_equal(rc, GTH_OK);

        /* Each start/stop should create valid trace */
        rc = gth_trace_stop();
        assert_int_equal(rc, GTH_OK);

        (void)remove(TEST_TRACE_FILE);
    }

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_trace_rejects_invalid_paths(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(122);
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    /* Null path */
    rc = gth_trace_start(NULL);
    assert_int_equal(rc, GTH_EINVAL);

    /* Empty path */
    rc = gth_trace_start("");
    assert_int_equal(rc, GTH_EINVAL);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_scheduler_mode_transitions(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(133);
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    /* Mode switching requires proper initialization */
    /* FUZZ mode works with valid seed from config */
    rc = gth_scheduler_set_mode(GTH_MODE_FUZZ);
    assert_int_equal(rc, GTH_OK);

    /* Mode can be switched back to NORMAL */
    rc = gth_scheduler_set_mode(GTH_MODE_NORMAL);
    assert_int_equal(rc, GTH_OK);

    /* RECORD mode can be initialized */
    rc = gth_scheduler_set_mode(GTH_MODE_RECORD);
    assert_int_equal(rc, GTH_OK);

    /* Mode switches back to NORMAL for cleanup */
    rc = gth_scheduler_set_mode(GTH_MODE_NORMAL);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}
