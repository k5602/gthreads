
#include <stdlib.h>
#include <string.h>

#include "runtime_state.h"
#include "trace_format.h"

/*
 * Trace Buffer Implementation
 *
 * Simple linear buffer that accumulates trace records and flushes
 * to file when full. Uses malloc/free for portability.
 *
 * Design: Append-only, no circular behavior - just flush when full.
 * This simplifies the code and ensures sequential file layout.
 *
 * Note: gth_trace_buffer_t is defined in runtime_state.h
 */

/*
 * Create a new trace buffer with specified capacity
 */
gth_trace_buffer_t *gth_trace_buffer_create(size_t capacity, size_t record_size)
{
    gth_trace_buffer_t *buf = NULL;
    size_t total_size = 0;

    if (capacity == 0 || record_size == 0)
    {
        return NULL;
    }

    /* Check for overflow in size calculation */
    if (record_size > (SIZE_MAX / capacity))
    {
        return NULL;
    }

    buf = (gth_trace_buffer_t *)malloc(sizeof(gth_trace_buffer_t));
    if (buf == NULL)
    {
        return NULL;
    }

    total_size = capacity * record_size;
    buf->buffer = (uint8_t *)malloc(total_size);
    if (buf->buffer == NULL)
    {
        free(buf);
        return NULL;
    }

    buf->capacity = capacity;
    buf->count = 0;
    buf->record_size = record_size;

    return buf;
}

/*
 * Destroy trace buffer and free memory
 */
void gth_trace_buffer_destroy(gth_trace_buffer_t *buf)
{
    if (buf == NULL)
    {
        return;
    }

    if (buf->buffer != NULL)
    {
        free(buf->buffer);
        buf->buffer = NULL;
    }

    free(buf);
}

/*
 * Check if buffer is full
 */
int gth_trace_buffer_is_full(const gth_trace_buffer_t *buf)
{
    if (buf == NULL)
    {
        return 0;
    }

    return buf->count >= buf->capacity;
}

/*
 * Check if buffer has any records
 */
int gth_trace_buffer_is_empty(const gth_trace_buffer_t *buf)
{
    if (buf == NULL)
    {
        return 1;
    }

    return buf->count == 0;
}

/*
 * Get number of records in buffer
 */
size_t gth_trace_buffer_count(const gth_trace_buffer_t *buf)
{
    if (buf == NULL)
    {
        return 0;
    }

    return buf->count;
}

/*
 * Append a record to buffer
 * Returns 1 on success, 0 if buffer is full
 */
int gth_trace_buffer_append(gth_trace_buffer_t *buf, const void *record)
{
    size_t offset = 0;

    if (buf == NULL || record == NULL)
    {
        return 0;
    }

    if (gth_trace_buffer_is_full(buf))
    {
        return 0;
    }

    offset = buf->count * buf->record_size;
    memcpy(buf->buffer + offset, record, buf->record_size);
    buf->count += 1;

    return 1;
}

/*
 * Flush buffer contents to file
 * Returns number of records written, or 0 on error
 */
size_t gth_trace_buffer_flush(gth_trace_buffer_t *buf, FILE *fp)
{
    size_t write_size = 0;
    size_t written = 0;

    if (buf == NULL || fp == NULL)
    {
        return 0;
    }

    if (buf->count == 0)
    {
        return 0;
    }

    write_size = buf->count * buf->record_size;
    written = fwrite(buf->buffer, 1, write_size, fp);

    if (written != write_size)
    {
        return 0;
    }

    /* Clear buffer after successful write */
    buf->count = 0;

    return written / buf->record_size;
}

/*
 * Clear buffer without writing
 */
void gth_trace_buffer_clear(gth_trace_buffer_t *buf)
{
    if (buf == NULL)
    {
        return;
    }

    buf->count = 0;
}
