#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "runtime_state.h"
#include "trace_format.h"

/* Forward declarations for buffer functions (from trace_buffer.c) */
/* Note: gth_trace_buffer_t is defined in runtime_state.h */
gth_trace_buffer_t *gth_trace_buffer_create(size_t capacity, size_t record_size);
void gth_trace_buffer_destroy(gth_trace_buffer_t *buf);
int gth_trace_buffer_append(gth_trace_buffer_t *buf, const void *record);
size_t gth_trace_buffer_flush(gth_trace_buffer_t *buf, FILE *fp);
void gth_trace_buffer_clear(gth_trace_buffer_t *buf);

/*
 * Get monotonic timestamp for trace records
 * Uses rdtsc on x86_64, falls back to clock_gettime
 */
static uint64_t gth_trace_timestamp(void)
{
#if defined(__x86_64__)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0;
    }
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
#endif
}

/*
 * Get timestamp frequency (for converting to time if needed)
 */
static uint64_t gth_trace_timestamp_freq(void)
{
#if defined(__x86_64__)
    /* Assume 2.5 GHz as common default - actual value not critical */
    return 2500000000ULL;
#else
    return 1000000000ULL; /* 1 GHz for nanosecond timestamps */
#endif
}

/*
 * Initialize trace subsystem
 */
gth_status_t gth_trace_init(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (state->trace != NULL)
    {
        return GTH_ESTATE; /* Already tracing */
    }

    state->trace = (gth_trace_state_t *)malloc(sizeof(gth_trace_state_t));
    if (state->trace == NULL)
    {
        return GTH_ENOMEM;
    }

    state->trace->file = NULL;
    state->trace->buffer = NULL;
    state->trace->event_count = 0;
    state->trace->timestamp_freq = gth_trace_timestamp_freq();
    state->trace->start_time = 0;
    state->trace->active = 0;

    return GTH_OK;
}

/*
 * Cleanup trace subsystem
 */
void gth_trace_cleanup(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->trace == NULL)
    {
        return;
    }

    if (state->trace->active)
    {
        gth_trace_flush();
    }

    if (state->trace->buffer != NULL)
    {
        gth_trace_buffer_destroy(state->trace->buffer);
        state->trace->buffer = NULL;
    }

    if (state->trace->file != NULL)
    {
        fclose(state->trace->file);
    }

    free(state->trace);
    state->trace = NULL;
    state->trace_enabled = 0;
}

/*
 * Write trace file header
 */
static gth_status_t gth_trace_write_header(FILE *fp)
{
    gth_trace_header_t header;
    struct timespec ts;
    size_t written;

    if (fp == NULL)
    {
        return GTH_EINVAL;
    }

    memset(&header, 0, sizeof(header));
    header.magic = GTH_TRACE_MAGIC;
    header.version = GTH_TRACE_VERSION;
    header.timestamp_freq = gth_trace_timestamp_freq();

    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
    {
        header.creation_time = ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
    }

    written = fwrite(&header, sizeof(header), 1, fp);
    if (written != 1)
    {
        return GTH_EINTERNAL;
    }

    return GTH_OK;
}

/*
 * Public API: Start tracing to file
 */
gth_status_t gth_trace_start(const char *trace_path)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_status_t status;

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (trace_path == NULL || trace_path[0] == '\0')
    {
        return GTH_EINVAL;
    }

    /* Initialize trace subsystem if not already */
    status = gth_trace_init();
    if (status != GTH_OK && status != GTH_ESTATE)
    {
        return status;
    }

    if (state->trace->file != NULL)
    {
        return GTH_ESTATE; /* Already tracing to a file */
    }

    /* Open trace file */
    state->trace->file = fopen(trace_path, "wb");
    if (state->trace->file == NULL)
    {
        gth_trace_cleanup();
        return GTH_EINTERNAL;
    }

    /* Write binary header */
    status = gth_trace_write_header(state->trace->file);
    if (status != GTH_OK)
    {
        fclose(state->trace->file);
        state->trace->file = NULL;
        gth_trace_cleanup();
        return status;
    }

    /* Create event buffer */
    state->trace->buffer = (gth_trace_buffer_t *)gth_trace_buffer_create(GTH_TRACE_BUFFER_COUNT,
                                                                         GTH_TRACE_RECORD_SIZE);
    if (state->trace->buffer == NULL)
    {
        fclose(state->trace->file);
        state->trace->file = NULL;
        gth_trace_cleanup();
        return GTH_ENOMEM;
    }

    state->trace->start_time = gth_trace_timestamp();
    state->trace->active = 1;
    state->trace_enabled = 1;

    /* Record runtime init event */
    gth_trace_record(GTH_EVT_RUNTIME_INIT, 0, 0, 0);

    return GTH_OK;
}

/*
 * Public API: Stop tracing
 */
gth_status_t gth_trace_stop(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (state->trace == NULL || !state->trace->active)
    {
        return GTH_ESTATE;
    }

    /* Record shutdown event */
    gth_trace_record(GTH_EVT_RUNTIME_SHUTDOWN, 0, 0, 0);

    /* Flush remaining events */
    gth_trace_flush();

    if (state->trace->file != NULL)
    {
        fclose(state->trace->file);
        state->trace->file = NULL;
    }

    state->trace->active = 0;
    state->trace_enabled = 0;

    /* Clean up trace resources */
    gth_trace_cleanup();

    return GTH_OK;
}

/*
 * Flush buffered events to file
 */
void gth_trace_flush(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->trace == NULL || state->trace->file == NULL)
    {
        return;
    }

    if (state->trace->buffer != NULL)
    {
        gth_trace_buffer_flush(state->trace->buffer, state->trace->file);
    }

    fflush(state->trace->file);
}

/*
 * Check if tracing is active
 */
int gth_trace_is_active(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->trace == NULL)
    {
        return 0;
    }

    return state->trace->active;
}

/*
 * Record an event to the trace
 *
 * This is the core function that all instrumentation calls.
 * Events are buffered and flushed periodically.
 */
void gth_trace_record(uint8_t type, gth_tid_t tid, uint64_t data1, uint64_t data2)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_trace_record_t record;
    gth_trace_buffer_t *buf;

    if (state == NULL || state->trace == NULL || !state->trace->active)
    {
        return;
    }

    /* Only record in RECORD mode or NORMAL mode with tracing enabled */
    if (state->mode == GTH_MODE_REPLAY)
    {
        return;
    }

    buf = (gth_trace_buffer_t *)state->trace->buffer;
    if (buf == NULL)
    {
        return;
    }

    /* Build record */
    memset(&record, 0, sizeof(record));
    record.timestamp = gth_trace_timestamp();
    record.type = type;
    record.tid = (uint32_t)tid;
    record.checksum = 0;

    /* Pack data into raw field based on event type */
    switch (type)
    {
    case GTH_EVT_CONTEXT_SWITCH:
        /* Pack from_tid in upper 32 bits, to_tid in lower 32 bits */
        record.data.raw = ((data1 & 0xFFFFFFFFULL) << 32) | (data2 & 0xFFFFFFFFULL);
        break;

    case GTH_EVT_THREAD_EXIT:
    case GTH_EVT_MUTEX_LOCK:
    case GTH_EVT_MUTEX_UNLOCK:
    case GTH_EVT_MUTEX_WAIT:
    case GTH_EVT_MUTEX_WAKE:
    case GTH_EVT_COND_WAIT:
        record.data.raw = data1;
        break;

    case GTH_EVT_SEM_WAIT:
    case GTH_EVT_SEM_POST:
    case GTH_EVT_COND_BROADCAST:
        /* Pack address in upper 32 bits, count in lower 32 bits */
        record.data.raw = ((data1 & 0xFFFFFFFFULL) << 32) | (data2 & 0xFFFFFFFFULL);
        break;

    case GTH_EVT_COND_SIGNAL:
        /* Pack cond_addr in upper 32 bits, target_tid in lower 32 bits */
        record.data.raw = ((data1 & 0xFFFFFFFFULL) << 32) | (data2 & 0xFFFFFFFFULL);
        break;

    default:
        record.data.raw = data1;
        break;
    }

    /* Calculate checksum */
    record.checksum = gth_trace_calc_checksum(&record);

    /* Try to append to buffer */
    if (!gth_trace_buffer_append(buf, &record))
    {
        /* Buffer full - flush and retry */
        gth_trace_flush();

        if (!gth_trace_buffer_append(buf, &record))
        {
            /* Still failed after flush - drop event */
            return;
        }
    }

    state->trace->event_count++;
}

/*
 * Record a context switch event
 * Called from scheduler when switching threads
 */
void gth_trace_context_switch(gth_tid_t from_tid, gth_tid_t to_tid)
{
    gth_trace_record(GTH_EVT_CONTEXT_SWITCH, from_tid, (uint64_t)from_tid, (uint64_t)to_tid);
}

/*
 * Record thread creation
 */
void gth_trace_thread_create(gth_tid_t tid)
{
    gth_trace_record(GTH_EVT_THREAD_CREATE, tid, 0, 0);
}

/*
 * Record thread exit
 */
void gth_trace_thread_exit(gth_tid_t tid, void *retval)
{
    uint64_t val = (uint64_t)(uintptr_t)retval;
    gth_trace_record(GTH_EVT_THREAD_EXIT, tid, val, 0);
}

/*
 * Record thread cancel
 */
void gth_trace_thread_cancel(gth_tid_t target_tid)
{
    gth_trace_record(GTH_EVT_THREAD_CANCEL, target_tid, 0, 0);
}

/*
 * Record thread block
 */
void gth_trace_thread_block(gth_tid_t tid, gth_block_reason_t reason)
{
    gth_trace_record(GTH_EVT_THREAD_BLOCK, tid, (uint64_t)reason, 0);
}

/*
 * Record thread unblock
 */
void gth_trace_thread_unblock(gth_tid_t tid)
{
    gth_trace_record(GTH_EVT_THREAD_UNBLOCK, tid, 0, 0);
}

/*
 * Record thread yield
 */
void gth_trace_thread_yield(gth_tid_t tid)
{
    gth_trace_record(GTH_EVT_THREAD_YIELD, tid, 0, 0);
}

/*
 * Record mutex operations
 */
void gth_trace_mutex_lock(gth_tid_t tid, const void *mutex_addr)
{
    gth_trace_record(GTH_EVT_MUTEX_LOCK, tid, (uint64_t)(uintptr_t)mutex_addr, 0);
}

void gth_trace_mutex_unlock(gth_tid_t tid, const void *mutex_addr)
{
    gth_trace_record(GTH_EVT_MUTEX_UNLOCK, tid, (uint64_t)(uintptr_t)mutex_addr, 0);
}

void gth_trace_mutex_wait(gth_tid_t tid, const void *mutex_addr)
{
    gth_trace_record(GTH_EVT_MUTEX_WAIT, tid, (uint64_t)(uintptr_t)mutex_addr, 0);
}

void gth_trace_mutex_wake(gth_tid_t tid, const void *mutex_addr)
{
    gth_trace_record(GTH_EVT_MUTEX_WAKE, tid, (uint64_t)(uintptr_t)mutex_addr, 0);
}

/*
 * Record semaphore operations
 */
void gth_trace_sem_wait(gth_tid_t tid, const void *sem_addr, uint32_t count_before)
{
    gth_trace_record(GTH_EVT_SEM_WAIT, tid, (uint64_t)(uintptr_t)sem_addr, (uint64_t)count_before);
}

void gth_trace_sem_post(gth_tid_t tid, const void *sem_addr, uint32_t count_after)
{
    gth_trace_record(GTH_EVT_SEM_POST, tid, (uint64_t)(uintptr_t)sem_addr, (uint64_t)count_after);
}

void gth_trace_sem_wake(gth_tid_t tid, const void *sem_addr)
{
    gth_trace_record(GTH_EVT_SEM_WAKE, tid, (uint64_t)(uintptr_t)sem_addr, 0);
}

/*
 * Record condition variable operations
 */
void gth_trace_cond_wait(gth_tid_t tid, const void *cond_addr)
{
    gth_trace_record(GTH_EVT_COND_WAIT, tid, (uint64_t)(uintptr_t)cond_addr, 0);
}

void gth_trace_cond_signal(gth_tid_t tid, const void *cond_addr, gth_tid_t target_tid)
{
    gth_trace_record(GTH_EVT_COND_SIGNAL, tid, (uint64_t)(uintptr_t)cond_addr,
                     (uint64_t)target_tid);
}

void gth_trace_cond_broadcast(gth_tid_t tid, const void *cond_addr, uint32_t wake_count)
{
    gth_trace_record(GTH_EVT_COND_BROADCAST, tid, (uint64_t)(uintptr_t)cond_addr,
                     (uint64_t)wake_count);
}

/*
 * Public API: Replay from trace file
 *
 * This initializes the replay system and switches to REPLAY mode.
 */
gth_status_t gth_replay_from(const char *trace_path)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_status_t status;

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (trace_path == NULL || trace_path[0] == '\0')
    {
        return GTH_EINVAL;
    }

    /* Initialize replay subsystem */
    status = gth_replay_init(trace_path);
    if (status != GTH_OK)
    {
        return status;
    }

    /* Switch to replay mode */
    state->mode = GTH_MODE_REPLAY;
    state->trace_enabled = 0; /* Don't record during replay */

    return GTH_OK;
}
