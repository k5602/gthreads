#ifndef GTHREADS_INTERNAL_WAIT_QUEUE_H
#define GTHREADS_INTERNAL_WAIT_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#define GTH_WQ_FIXED_SLOTS 52U

typedef struct
{
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint8_t capacity;
    uint8_t slots[GTH_WQ_FIXED_SLOTS];
} gth_wait_queue_t;

static inline void gth_wq_init(gth_wait_queue_t *wq, uint8_t capacity)
{
    wq->head = 0;
    wq->tail = 0;
    wq->count = 0;
    wq->capacity = capacity;
}

static inline int gth_wq_is_empty(const gth_wait_queue_t *wq)
{
    return wq->count == 0;
}

static inline int gth_wq_is_full(const gth_wait_queue_t *wq)
{
    return wq->count >= wq->capacity;
}

static inline void gth_wq_enqueue(gth_wait_queue_t *wq, uint8_t slot_index)
{
    if (wq->count >= wq->capacity)
    {
        return;
    }

    wq->slots[wq->tail] = slot_index;
    wq->tail = (wq->tail + 1) % wq->capacity;
    wq->count += 1;
}

static inline uint8_t gth_wq_dequeue(gth_wait_queue_t *wq)
{
    uint8_t slot_index;

    if (wq->count == 0)
    {
        return 0xFF;
    }

    slot_index = wq->slots[wq->head];
    wq->head = (wq->head + 1) % wq->capacity;
    wq->count -= 1;
    return slot_index;
}

/* Remove the first occurrence of slot_value from the queue.
 * Returns 1 if removed, 0 if not found. O(n) scan -- acceptable
 * because capacity is small (<=52) and this is only called on
 * error paths. */
static inline int gth_wq_remove(gth_wait_queue_t *wq, uint8_t slot_value)
{
    if (wq->count == 0)
    {
        return 0;
    }

    for (uint8_t i = 0; i < wq->count; i++)
    {
        uint8_t idx = (wq->head + i) % wq->capacity;
        if (wq->slots[idx] == slot_value)
        {
            /* Shift elements [idx+1 .. tail) left by one slot. */
            for (uint8_t j = i; j + 1 < wq->count; j++)
            {
                uint8_t cur = (wq->head + j) % wq->capacity;
                uint8_t nxt = (wq->head + j + 1) % wq->capacity;
                wq->slots[cur] = wq->slots[nxt];
            }
            wq->tail = (wq->tail + wq->capacity - 1) % wq->capacity;
            wq->count -= 1;
            return 1;
        }
    }
    return 0;
}

#endif
