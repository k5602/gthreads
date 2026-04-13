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

#endif
