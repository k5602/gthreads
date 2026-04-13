#ifndef GTHREADS_INTERNAL_RUNTIME_STATE_H
#define GTHREADS_INTERNAL_RUNTIME_STATE_H

#include <stddef.h>
#include <stdint.h>
#include <ucontext.h>

#include "gthreads/gthreads.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define GTH_MAX_THREADS 128U

#define GTH_STACK_GUARD_PAGES 1U

    typedef enum
    {
        GTH_THREAD_EMPTY = 0,
        GTH_THREAD_READY,
        GTH_THREAD_RUNNING,
        GTH_THREAD_BLOCKED,
        GTH_THREAD_DONE,
        GTH_THREAD_CANCELED
    } gth_thread_state_t;

    typedef struct
    {
        void *memory;
        size_t total_size;
        size_t stack_size;
        void *stack_top;
        void *guard_page;
    } gth_stack_allocation_t;

    typedef struct
    {
        gth_tid_t tid;
        gth_thread_fn fn;
        void *arg;
        void *retval;
        uint32_t priority;
        gth_thread_state_t state;
        ucontext_t ctx;
        gth_stack_allocation_t stack;
        size_t slot_index;
    } gth_thread_record_t;

    typedef struct
    {
        int initialized;
        int shutting_down;
        gth_runtime_config_t config;
        gth_tid_t next_tid;
        gth_tid_t current_tid;
        uint64_t context_switches;
        uint64_t runnable_threads;
        uint64_t blocked_threads;
        uint64_t finished_threads;
        int trace_enabled;
        ucontext_t scheduler_ctx;
        gth_thread_record_t threads[GTH_MAX_THREADS];
    } gth_runtime_state_t;

    typedef struct
    {
        uint64_t total;
        uint64_t runnable;
        uint64_t blocked;
        uint64_t finished;
    } gth_runtime_stats_snapshot_t;

    gth_runtime_state_t *gth_runtime_state(void);
    gth_thread_record_t *gth_runtime_find_thread(gth_runtime_state_t *state, gth_tid_t tid);
    gth_thread_record_t *gth_runtime_alloc_thread_slot(gth_runtime_state_t *state);
    int gth_thread_is_terminal(gth_thread_state_t state);

    gth_status_t gth_scheduler_run_next(void);
    gth_status_t gth_scheduler_run_until(gth_tid_t tid);

    gth_status_t gth_stack_allocate(size_t stack_size_bytes, gth_stack_allocation_t *out_stack);
    void gth_stack_free(gth_stack_allocation_t *stack);

    gth_status_t gth_context_init_thread(ucontext_t *ctx, const gth_stack_allocation_t *stack,
                                         size_t slot_index);
    void gth_context_thread_trampoline(int slot_index_arg);

    gth_status_t gth_thread_block(void);
    gth_status_t gth_thread_unblock_slot(size_t slot_index);
    size_t gth_thread_current_slot_index(void);

    void gth_runtime_snapshot_stats(gth_runtime_stats_snapshot_t *snapshot);
    gth_status_t gth_runtime_begin_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
