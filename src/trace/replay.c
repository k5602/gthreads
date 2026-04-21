
/*
 * Deterministic Replay Module
 *
 * Loads a trace file and drives the scheduler to reproduce
 * an exact execution. Detects divergence when the actual
 * execution differs from the recorded trace.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "runtime_state.h"
#include "trace_format.h"

/*
 * Default maximum trace file size: 256 MB
 * This limits memory usage when loading traces.
 */
#define GTH_REPLAY_MAX_FILE_SIZE (256U * 1024U * 1024U)

/*
 * Validate trace file header
 */
static int gth_replay_validate_header(const gth_trace_header_t *header)
{
    if (header == NULL)
    {
        return 0;
    }

    if (header->magic != GTH_TRACE_MAGIC)
    {
        return 0;
    }

    if (header->version != GTH_TRACE_VERSION)
    {
        return 0;
    }

    return 1;
}

/*
 * Read entire file into allocated memory
 * Returns pointer to data, sets out_size to bytes read.
 * Caller must free the returned pointer.
 */
static uint8_t *gth_replay_read_file(const char *path, size_t *out_size)
{
    FILE *fp = NULL;
    long file_size = 0;
    size_t read_size = 0;
    uint8_t *data = NULL;

    if (path == NULL || out_size == NULL)
    {
        return NULL;
    }

    *out_size = 0;

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return NULL;
    }

    /* Determine file size */
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return NULL;
    }

    file_size = ftell(fp);
    if (file_size < 0)
    {
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    /* Sanity check file size */
    if ((size_t)file_size < sizeof(gth_trace_header_t))
    {
        fclose(fp);
        return NULL; /* Too small for header */
    }

    if ((size_t)file_size > GTH_REPLAY_MAX_FILE_SIZE)
    {
        fclose(fp);
        return NULL; /* Too large */
    }

    /* Allocate memory for file contents */
    data = (uint8_t *)malloc((size_t)file_size);
    if (data == NULL)
    {
        fclose(fp);
        return NULL;
    }

    /* Read entire file */
    read_size = fread(data, 1, (size_t)file_size, fp);
    fclose(fp);

    if (read_size != (size_t)file_size)
    {
        free(data);
        return NULL;
    }

    *out_size = read_size;
    return data;
}

/*
 * Validate all records in the trace
 * Returns number of valid records, or 0 on error
 */
static size_t gth_replay_count_records(const uint8_t *data, size_t data_size)
{
    const gth_trace_header_t *header;
    const gth_trace_record_t *records;
    size_t record_count = 0;
    size_t i;

    if (data == NULL || data_size < sizeof(gth_trace_header_t))
    {
        return 0;
    }

    header = (const gth_trace_header_t *)data;

    if (!gth_replay_validate_header(header))
    {
        return 0;
    }

    /* Calculate expected record count */
    record_count = (data_size - sizeof(gth_trace_header_t)) / GTH_TRACE_RECORD_SIZE;

    /* Verify each record */
    records = (const gth_trace_record_t *)(data + sizeof(gth_trace_header_t));

    for (i = 0; i < record_count; ++i)
    {
        if (!gth_trace_validate_record(&records[i]))
        {
            return 0; /* Checksum mismatch */
        }
    }

    return record_count;
}

/*
 * Initialize replay from trace file
 */
gth_status_t gth_replay_init(const char *trace_path)
{
    gth_runtime_state_t *state = gth_runtime_state();
    size_t file_size = 0;
    size_t record_count = 0;
    uint8_t *data = NULL;

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (trace_path == NULL || trace_path[0] == '\0')
    {
        return GTH_EINVAL;
    }

    if (state->replay != NULL)
    {
        return GTH_ESTATE; /* Already replaying */
    }

    /* Read trace file */
    data = gth_replay_read_file(trace_path, &file_size);
    if (data == NULL)
    {
        return GTH_ENOTFOUND;
    }

    /* Validate records */
    record_count = gth_replay_count_records(data, file_size);
    if (record_count == 0)
    {
        free(data);
        return GTH_ESTATE; /* Invalid trace file */
    }

    /* Allocate replay state */
    state->replay = (gth_replay_state_t *)malloc(sizeof(gth_replay_state_t));
    if (state->replay == NULL)
    {
        free(data);
        return GTH_ENOMEM;
    }

    /* Initialize replay state */
    state->replay->events = data;
    state->replay->event_count = record_count;
    state->replay->current_idx = 0;
    state->replay->diverged = 0;
    state->replay->last_timestamp = 0;

    return GTH_OK;
}

/*
 * Cleanup replay state
 */
void gth_replay_cleanup(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->replay == NULL)
    {
        return;
    }

    if (state->replay->events != NULL)
    {
        free(state->replay->events);
    }

    free(state->replay);
    state->replay = NULL;
}

/*
 * Get the next event from the trace
 * Returns 1 on success, 0 if no more events
 */
int gth_replay_next_event(uint8_t *out_type, gth_tid_t *out_tid, uint64_t *out_data)
{
    gth_runtime_state_t *state = gth_runtime_state();
    const gth_trace_record_t *records;
    const gth_trace_record_t *rec;

    if (state == NULL || state->replay == NULL)
    {
        return 0;
    }

    if (state->replay->current_idx >= state->replay->event_count)
    {
        return 0; /* End of trace */
    }

    records = (const gth_trace_record_t *)(state->replay->events + sizeof(gth_trace_header_t));
    rec = &records[state->replay->current_idx];

    if (out_type != NULL)
    {
        *out_type = rec->type;
    }

    if (out_tid != NULL)
    {
        *out_tid = rec->tid;
    }

    if (out_data != NULL)
    {
        *out_data = rec->data.raw;
    }

    state->replay->last_timestamp = rec->timestamp;
    state->replay->current_idx++;

    return 1;
}

/*
 * Peek at the next event without consuming it
 * Returns 1 on success, 0 if no more events
 */
static int gth_replay_peek_event(uint8_t *out_type, gth_tid_t *out_tid, uint64_t *out_data)
{
    gth_runtime_state_t *state = gth_runtime_state();
    const gth_trace_record_t *records;
    const gth_trace_record_t *rec;
    size_t idx;

    if (state == NULL || state->replay == NULL)
    {
        return 0;
    }

    idx = state->replay->current_idx;
    if (idx >= state->replay->event_count)
    {
        return 0;
    }

    records = (const gth_trace_record_t *)(state->replay->events + sizeof(gth_trace_header_t));
    rec = &records[idx];

    if (out_type != NULL)
    {
        *out_type = rec->type;
    }

    if (out_tid != NULL)
    {
        *out_tid = rec->tid;
    }

    if (out_data != NULL)
    {
        *out_data = rec->data.raw;
    }

    return 1;
}

/*
 * Check if replay has diverged from trace
 */
int gth_replay_has_diverged(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->replay == NULL)
    {
        return 0;
    }

    return state->replay->diverged;
}

/*
 * Mark replay as having diverged
 */
void gth_replay_mark_diverged(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->replay == NULL)
    {
        return;
    }

    state->replay->diverged = 1;
}

gth_status_t gth_replay_get_stats(gth_replay_stats_t *out_stats)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (out_stats == NULL)
    {
        return GTH_EINVAL;
    }

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    if (state->replay == NULL)
    {
        return GTH_ESTATE; /* Replay not active */
    }

    out_stats->event_count = state->replay->event_count;
    out_stats->current_idx = state->replay->current_idx;
    out_stats->diverged = state->replay->diverged;

    return GTH_OK;
}

/*
 * Find thread by TID, checking state constraints
 */
static gth_thread_record_t *gth_replay_find_thread(gth_runtime_state_t *state, gth_tid_t tid)
{
    gth_thread_record_t *thread;

    if (state == NULL || tid == 0)
    {
        return NULL;
    }

    thread = gth_runtime_find_thread(state, tid);

    if (thread == NULL)
    {
        return NULL;
    }

    /* For replay, we want ready or running threads */
    if (thread->state != GTH_THREAD_READY && thread->state != GTH_THREAD_RUNNING)
    {
        return NULL;
    }

    return thread;
}

/*
 * Get the next thread to run according to the trace
 *
 * This is called instead of the normal scheduler when in REPLAY mode.
 * It reads the next event from the trace and returns the specified thread.
 */
gth_thread_record_t *gth_replay_next_thread(gth_runtime_state_t *state)
{
    uint8_t event_type = GTH_EVT_NONE;
    gth_tid_t tid = 0;
    uint64_t data = 0;
    gth_thread_record_t *thread = NULL;

    if (state == NULL || state->replay == NULL)
    {
        return NULL;
    }

    /* Skip non-scheduling events until we find a context switch */
    while (gth_replay_peek_event(&event_type, &tid, &data))
    {
        if (event_type == GTH_EVT_CONTEXT_SWITCH)
        {
            break;
        }

        /* Consume non-scheduling events */
        gth_replay_next_event(NULL, NULL, NULL);
    }

    if (!gth_replay_peek_event(&event_type, &tid, &data))
    {
        return NULL; /* No more events */
    }

    if (event_type != GTH_EVT_CONTEXT_SWITCH)
    {
        return NULL; /* Unexpected event type */
    }

    /* Get the 'to_tid' from the context switch event */
    tid = (gth_tid_t)(data & 0xFFFFFFFFU);

    /* Find the target thread */
    thread = gth_replay_find_thread(state, tid);
    if (thread == NULL)
    {
        /* Thread not found or not ready - divergence */
        gth_replay_mark_diverged();
        return NULL;
    }

    /* Consume this event */
    gth_replay_next_event(NULL, NULL, NULL);

    return thread;
}

/*
 * Validate that a synchronization event matches the trace
 *
 * Call this from sync primitives to verify execution matches trace.
 * Returns 1 if event matches, 0 if diverged.
 */
int gth_replay_validate_event(uint8_t expected_type, gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();
    uint8_t actual_type = GTH_EVT_NONE;
    gth_tid_t actual_tid = 0;

    if (state == NULL || state->replay == NULL)
    {
        return 1; /* Not replaying, so no validation needed */
    }

    if (!gth_replay_peek_event(&actual_type, &actual_tid, NULL))
    {
        gth_replay_mark_diverged();
        return 0; /* Trace ended unexpectedly */
    }

    if (actual_type != expected_type || actual_tid != tid)
    {
        gth_replay_mark_diverged();
        return 0; /* Event mismatch */
    }

    /* Consume the validated event */
    gth_replay_next_event(NULL, NULL, NULL);
    return 1;
}
