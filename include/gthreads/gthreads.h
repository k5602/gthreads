#ifndef GTHREADS_GTHREADS_H
#define GTHREADS_GTHREADS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint64_t gth_tid_t;

    typedef enum
    {
        GTH_OK = 0,
        GTH_EINVAL,
        GTH_ENOMEM,
        GTH_ESTATE,
        GTH_EBUSY,
        GTH_ETIMEDOUT,
        GTH_ENOTFOUND,
        GTH_EINTERNAL,
    } gth_status_t;

    typedef enum
    {
        GTH_SCHED_RR = 0,
        GTH_SCHED_PRIORITY,
    } gth_sched_policy_t;

    typedef struct
    {
        size_t stack_size_bytes;
        gth_sched_policy_t policy;
        uint32_t quantum_us;
        uint32_t replay_seed;
        int enable_deterministic_trace;
        int enable_schedule_fuzzing;
    } gth_runtime_config_t;

    typedef struct
    {
        gth_tid_t tid;
        uint32_t priority;
        const char *name;
    } gth_thread_attr_t;

    typedef struct
    {
        uint64_t _opaque[8];
    } gth_mutex_t;

    typedef struct
    {
        uint64_t _opaque[8];
    } gth_sem_t;

    typedef struct
    {
        uint64_t _opaque[8];
    } gth_cond_t;

    typedef void *(*gth_thread_fn)(void *arg);

    gth_status_t gth_runtime_init(const gth_runtime_config_t *config);
    gth_status_t gth_runtime_shutdown(void);

    gth_status_t gth_thread_create(gth_tid_t *out_tid, const gth_thread_attr_t *attr,
                                   gth_thread_fn fn, void *arg);
    gth_status_t gth_thread_yield(void);
    gth_status_t gth_thread_join(gth_tid_t tid, void **retval);
    gth_status_t gth_thread_cancel(gth_tid_t tid);
    gth_tid_t gth_thread_self(void);

    gth_status_t gth_mutex_init(gth_mutex_t *m);
    gth_status_t gth_mutex_lock(gth_mutex_t *m);
    gth_status_t gth_mutex_trylock(gth_mutex_t *m);
    gth_status_t gth_mutex_unlock(gth_mutex_t *m);
    gth_status_t gth_mutex_destroy(gth_mutex_t *m);

    gth_status_t gth_sem_init(gth_sem_t *s, uint32_t initial_count);
    gth_status_t gth_sem_wait(gth_sem_t *s);
    gth_status_t gth_sem_post(gth_sem_t *s);
    gth_status_t gth_sem_destroy(gth_sem_t *s);

    gth_status_t gth_cond_init(gth_cond_t *c);
    gth_status_t gth_cond_wait(gth_cond_t *c, gth_mutex_t *m);
    gth_status_t gth_cond_signal(gth_cond_t *c);
    gth_status_t gth_cond_broadcast(gth_cond_t *c);
    gth_status_t gth_cond_destroy(gth_cond_t *c);

    gth_status_t gth_trace_start(const char *trace_path);
    gth_status_t gth_trace_stop(void);
    gth_status_t gth_replay_from(const char *trace_path);

    typedef struct
    {
        uint64_t context_switches;
        uint64_t runnable_threads;
        uint64_t blocked_threads;
    } gth_runtime_stats_t;

    gth_status_t gth_runtime_get_stats(gth_runtime_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif
