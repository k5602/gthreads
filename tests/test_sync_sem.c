#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "gthreads/gthreads.h"
#include "test_support.h"

#define PRODUCER_COUNT 3
#define CONSUMER_COUNT 3
#define ITEMS_PER_PRODUCER 4

static gth_sem_t g_sem_items;
static gth_sem_t g_sem_spaces;
static gth_mutex_t g_buf_mutex;
static volatile int g_ring_buf[16];
static volatile int g_buf_head;
static volatile int g_buf_tail;
static volatile int g_produced_total;
static volatile int g_consumed_total;
static const int g_buf_capacity = 16;

static void *sem_producer_fn(void *arg)
{
    int base = *(int *)arg;
    (void)arg;

    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i)
    {
        gth_sem_wait(&g_sem_spaces);
        gth_mutex_lock(&g_buf_mutex);
        g_ring_buf[g_buf_tail] = base + i;
        g_buf_tail = (g_buf_tail + 1) % g_buf_capacity;
        g_produced_total += 1;
        gth_mutex_unlock(&g_buf_mutex);
        gth_sem_post(&g_sem_items);
    }

    return NULL;
}

static void *sem_consumer_fn(void *arg)
{
    (void)arg;

    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i)
    {
        gth_sem_wait(&g_sem_items);
        gth_mutex_lock(&g_buf_mutex);
        g_ring_buf[g_buf_head] = 0;
        g_buf_head = (g_buf_head + 1) % g_buf_capacity;
        g_consumed_total += 1;
        gth_mutex_unlock(&g_buf_mutex);
        gth_sem_post(&g_sem_spaces);
    }

    return NULL;
}

void test_sem_producer_consumer(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(99);
    gth_tid_t producers[PRODUCER_COUNT];
    gth_tid_t consumers[CONSUMER_COUNT];
    int bases[PRODUCER_COUNT];
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_init(&g_sem_items, 0);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_init(&g_sem_spaces, (uint32_t)g_buf_capacity);
    assert_int_equal(rc, GTH_OK);
    rc = gth_mutex_init(&g_buf_mutex);
    assert_int_equal(rc, GTH_OK);

    g_buf_head = 0;
    g_buf_tail = 0;
    g_produced_total = 0;
    g_consumed_total = 0;

    for (int i = 0; i < PRODUCER_COUNT; ++i)
    {
        bases[i] = i * 100;
    }

    for (int i = 0; i < PRODUCER_COUNT; ++i)
    {
        rc = gth_thread_create(&producers[i], NULL, sem_producer_fn, &bases[i]);
        assert_int_equal(rc, GTH_OK);
    }

    for (int i = 0; i < CONSUMER_COUNT; ++i)
    {
        rc = gth_thread_create(&consumers[i], NULL, sem_consumer_fn, NULL);
        assert_int_equal(rc, GTH_OK);
    }

    for (int i = 0; i < PRODUCER_COUNT; ++i)
    {
        rc = gth_thread_join(producers[i], NULL);
        assert_int_equal(rc, GTH_OK);
    }

    for (int i = 0; i < CONSUMER_COUNT; ++i)
    {
        rc = gth_thread_join(consumers[i], NULL);
        assert_int_equal(rc, GTH_OK);
    }

    assert_int_equal(g_produced_total, PRODUCER_COUNT * ITEMS_PER_PRODUCER);
    assert_int_equal(g_consumed_total, CONSUMER_COUNT * ITEMS_PER_PRODUCER);

    rc = gth_sem_destroy(&g_sem_items);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_destroy(&g_sem_spaces);
    assert_int_equal(rc, GTH_OK);
    rc = gth_mutex_destroy(&g_buf_mutex);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

static volatile int g_sem_order_buf[8];
static volatile int g_sem_order_idx;
static gth_sem_t g_sem_order_sem;

static void *sem_order_waiter_fn(void *arg)
{
    int id = *(int *)arg;
    gth_sem_wait(&g_sem_order_sem);
    g_sem_order_buf[g_sem_order_idx++] = id;
    return NULL;
}

void test_sem_fifo_wake_order(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(11);
    gth_tid_t tids[3];
    int ids[3] = {10, 20, 30};
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_init(&g_sem_order_sem, 0);
    assert_int_equal(rc, GTH_OK);

    g_sem_order_idx = 0;

    for (int i = 0; i < 3; ++i)
    {
        rc = gth_thread_create(&tids[i], NULL, sem_order_waiter_fn, &ids[i]);
        assert_int_equal(rc, GTH_OK);
    }

    gth_thread_yield();

    for (int i = 0; i < 3; ++i)
    {
        gth_sem_post(&g_sem_order_sem);
        gth_thread_yield();
    }

    for (int i = 0; i < 3; ++i)
    {
        rc = gth_thread_join(tids[i], NULL);
        assert_int_equal(rc, GTH_OK);
    }

    assert_int_equal(g_sem_order_buf[0], 10);
    assert_int_equal(g_sem_order_buf[1], 20);
    assert_int_equal(g_sem_order_buf[2], 30);

    rc = gth_sem_destroy(&g_sem_order_sem);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_sem_count_tracks_correctly(void **state)
{
    gth_runtime_config_t cfg = gth_test_make_rr_config(1);
    gth_sem_t s;
    int rc;

    (void)state;

    rc = gth_runtime_init(&cfg);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_init(&s, 3);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_wait(&s);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_wait(&s);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_post(&s);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_post(&s);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_post(&s);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_wait(&s);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_wait(&s);
    assert_int_equal(rc, GTH_OK);
    rc = gth_sem_wait(&s);
    assert_int_equal(rc, GTH_OK);

    rc = gth_sem_destroy(&s);
    assert_int_equal(rc, GTH_OK);

    rc = gth_runtime_shutdown();
    assert_int_equal(rc, GTH_OK);
}

void test_sem_rejects_null(void **state)
{
    (void)state;
    assert_int_equal(gth_sem_init(NULL, 0), GTH_EINVAL);
    assert_int_equal(gth_sem_wait(NULL), GTH_EINVAL);
    assert_int_equal(gth_sem_post(NULL), GTH_EINVAL);
    assert_int_equal(gth_sem_destroy(NULL), GTH_EINVAL);
}
