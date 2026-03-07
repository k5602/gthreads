#ifndef GTHREADS_INTERNAL_RUNTIME_STATE_H
#define GTHREADS_INTERNAL_RUNTIME_STATE_H

#include <stdint.h>

#include "gthreads/gthreads.h"

#define GTH_MAX_THREADS 1024U

typedef enum
{
    GTH_THREAD_EMPTY = 0,
    GTH_THREAD_READY,
    GTH_THREAD_RUNNING,
    GTH_THREAD_BLOCKED,
    GTH_THREAD_DONE,
    GTH_THREAD_CANCELED,
} gth_thread_state_t;

typedef struct
{
    gth_tid_t tid;
    gth_thread_fn fn;
    void *arg;
    void *retval;
    gth_thread_state_t state;
    uint32_t priority;
} gth_thread_record_t;

typedef struct
{
    int initialized;
    gth_runtime_config_t config;
    gth_tid_t next_tid;
    gth_tid_t current_tid;
    uint64_t context_switches;
    uint64_t runnable_threads;
    uint64_t blocked_threads;
    gth_thread_record_t threads[GTH_MAX_THREADS];
    int trace_enabled;
} gth_runtime_state_t;

gth_runtime_state_t *gth_runtime_state(void);
gth_thread_record_t *gth_runtime_find_thread(gth_runtime_state_t *state, gth_tid_t tid);
gth_thread_record_t *gth_runtime_alloc_thread_slot(gth_runtime_state_t *state);
int gth_thread_is_terminal(gth_thread_state_t state);

gth_status_t gth_scheduler_run_next(void);
gth_status_t gth_scheduler_run_until(gth_tid_t tid);

#endif
