#ifndef GTHREADS_INTERNAL_RUNTIME_STATE_H
#define GTHREADS_INTERNAL_RUNTIME_STATE_H

#include "../context/ctx.h"
#include "gthreads/gthreads.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../trace/trace_format.h"

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
        gth_ctx_t ctx;
        gth_stack_allocation_t stack;
        size_t slot_index;
    } gth_thread_record_t;

    /*
     * Scheduler operating modes for  trace/replay/fuzz
     */
    typedef enum
    {
        GTH_MODE_NORMAL = 0, /* Normal cooperative scheduling */
        GTH_MODE_RECORD = 1, /* Recording events to trace file */
        GTH_MODE_REPLAY = 2, /* Replaying from trace file */
        GTH_MODE_FUZZ = 3    /* Fuzzing schedule with PRNG perturbations */
    } gth_scheduler_mode_t;

    /*
     * Trace buffer for batching events before file write
     */
    typedef struct
    {
        uint8_t *buffer;    /* Raw buffer memory */
        size_t capacity;    /* Max records in buffer */
        size_t count;       /* Current records in buffer */
        size_t record_size; /* Bytes per record */
    } gth_trace_buffer_t;

    /*
     * Trace state - active during RECORD mode
     */
    typedef struct
    {
        FILE *file;                 /* Trace output file */
        gth_trace_buffer_t *buffer; /* Event buffer */
        uint64_t event_count;       /* Total events recorded */
        uint64_t timestamp_freq;    /* Ticks per second for timestamp calc */
        uint64_t start_time;        /* Timestamp of first event */
        int active;                 /* Tracing currently enabled */
    } gth_trace_state_t;

    /*
     * Replay state - active during REPLAY mode
     */
    typedef struct
    {
        uint8_t *events;         /* Loaded trace events */
        size_t event_count;      /* Total events in trace */
        size_t current_idx;      /* Next event to consume */
        int diverged;            /* Set if replay diverged from trace */
        uint64_t last_timestamp; /* Last timestamp processed */
    } gth_replay_state_t;

    /*
     * Fuzz state - active during FUZZ mode
     */
    typedef struct
    {
        uint64_t state[2];          /* xorshift128+ state */
        uint64_t seed;              /* Original seed for reproducibility */
        uint64_t decision_count;    /* Number of scheduling decisions made */
        uint32_t perturbation_rate; /* Percent chance of perturbation (0-100) */
    } gth_fuzz_state_t;

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
        /* Intentionally zero-initialized: scheduler_ctx.fxsave_area is NULL because
         * the scheduler must remain FPU-free. The scheduler never uses floating-point
         * or SSE state, so no fxsave buffer is needed or allocated.
         */
        gth_ctx_t scheduler_ctx;
        gth_thread_record_t threads[GTH_MAX_THREADS];
        size_t last_rr_slot;

        /* Scheduler mode and subsystems */
        gth_scheduler_mode_t mode;  /* Current operating mode */
        gth_trace_state_t *trace;   /* Active trace state (NULL if not recording) */
        gth_replay_state_t *replay; /* Active replay state (NULL if not replaying) */
        gth_fuzz_state_t *fuzz;     /* Active fuzz state (NULL if not fuzzing) */
    } gth_runtime_state_t;

    typedef struct
    {
        uint64_t total;
        uint64_t runnable;
        uint64_t blocked;
        uint64_t finished;
    } gth_runtime_stats_snapshot_t;

    /* Core runtime state */
    gth_runtime_state_t *gth_runtime_state(void);
    gth_thread_record_t *gth_runtime_find_thread(gth_runtime_state_t *state, gth_tid_t tid);
    gth_thread_record_t *gth_runtime_alloc_thread_slot(gth_runtime_state_t *state);
    int gth_thread_is_terminal(gth_thread_state_t state);

    /* Scheduler */
    gth_status_t gth_scheduler_run_next(void);
    gth_status_t gth_scheduler_run_until(gth_tid_t tid);
    gth_status_t gth_scheduler_set_mode(gth_scheduler_mode_t mode);

    /* Stack management */
    gth_status_t gth_stack_allocate(size_t stack_size_bytes, gth_stack_allocation_t *out_stack);
    void gth_stack_free(gth_stack_allocation_t *stack);

    /* Context switching */
    gth_status_t gth_context_init_thread(gth_ctx_t *ctx, const gth_stack_allocation_t *stack,
                                         size_t slot_index);
    void gth_context_destroy(gth_ctx_t *ctx);
    void gth_context_thread_trampoline(size_t slot_index_arg);

    /* Thread lifecycle */
    gth_status_t gth_thread_block(void);
    gth_status_t gth_thread_unblock_slot(size_t slot_index);
    size_t gth_thread_current_slot_index(void);

    /* Runtime management */
    void gth_runtime_snapshot_stats(gth_runtime_stats_snapshot_t *snapshot);
    gth_status_t gth_runtime_begin_shutdown(void);

    /* Trace system - record events */
    gth_status_t gth_trace_init(void);
    void gth_trace_cleanup(void);
    void gth_trace_record(uint8_t type, gth_tid_t tid, uint64_t data1, uint64_t data2);
    void gth_trace_flush(void);
    int gth_trace_is_active(void);

    /* Trace instrumentation - thread events */
    void gth_trace_context_switch(gth_tid_t from_tid, gth_tid_t to_tid);
    void gth_trace_thread_create(gth_tid_t tid);
    void gth_trace_thread_exit(gth_tid_t tid, void *retval);
    void gth_trace_thread_cancel(gth_tid_t target_tid);
    void gth_trace_thread_block(gth_tid_t tid, gth_block_reason_t reason);
    void gth_trace_thread_unblock(gth_tid_t tid);
    void gth_trace_thread_yield(gth_tid_t tid);

    /* Trace instrumentation - synchronization events */
    void gth_trace_mutex_lock(gth_tid_t tid, const void *mutex_addr);
    void gth_trace_mutex_unlock(gth_tid_t tid, const void *mutex_addr);
    void gth_trace_mutex_wait(gth_tid_t tid, const void *mutex_addr);
    void gth_trace_mutex_wake(gth_tid_t tid, const void *mutex_addr);

    void gth_trace_sem_wait(gth_tid_t tid, const void *sem_addr, uint32_t count_before);
    void gth_trace_sem_post(gth_tid_t tid, const void *sem_addr, uint32_t count_after);
    void gth_trace_sem_wake(gth_tid_t tid, const void *sem_addr);

    void gth_trace_cond_wait(gth_tid_t tid, const void *cond_addr);
    void gth_trace_cond_signal(gth_tid_t tid, const void *cond_addr, gth_tid_t target_tid);
    void gth_trace_cond_broadcast(gth_tid_t tid, const void *cond_addr, uint32_t wake_count);

    /* Replay validation */
    int gth_replay_validate_event(uint8_t expected_type, gth_tid_t tid);

    /* Replay system - deterministic replay */
    gth_status_t gth_replay_init(const char *trace_path);
    void gth_replay_cleanup(void);
    int gth_replay_next_event(uint8_t *out_type, gth_tid_t *out_tid, uint64_t *out_data);
    int gth_replay_has_diverged(void);
    void gth_replay_mark_diverged(void);
    gth_thread_record_t *gth_replay_next_thread(gth_runtime_state_t *state);

    /* Fuzz system - schedule perturbation */
    gth_status_t gth_fuzz_init(uint64_t seed);
    void gth_fuzz_cleanup(void);
    int gth_fuzz_should_perturb(void);
    uint64_t gth_fuzz_random(void);
    gth_thread_record_t *gth_fuzz_pick_thread(gth_runtime_state_t *state,
                                              gth_thread_record_t *normal_choice);

    /* Mode-aware scheduler selection */
    gth_thread_record_t *gth_scheduler_pick_ready_thread_mode(gth_runtime_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* GTHREADS_INTERNAL_RUNTIME_STATE_H */
