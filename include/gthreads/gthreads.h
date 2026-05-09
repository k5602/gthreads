#ifndef GTHREADS_GTHREADS_H
#define GTHREADS_GTHREADS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Thread identifier type. Each thread is assigned a unique monotonically
     * increasing TID at creation time. TID 0 is reserved and never assigned.
     */
    typedef uint64_t gth_tid_t;

    /**
     * Status codes returned by all public API functions. GTH_OK (0) indicates
     * success; any other value indicates a specific error condition.
     */
    typedef enum
    {
        GTH_OK = 0,
        GTH_EINVAL,    /**< Invalid argument (NULL pointer, out-of-range value) */
        GTH_ENOMEM,    /**< Memory allocation failed */
        GTH_ESTATE,    /**< Operation invalid in current runtime state */
        GTH_EBUSY,     /**< Resource is currently in use */
        GTH_ETIMEDOUT, /**< Operation timed out */
        GTH_ENOTFOUND, /**< Requested resource not found (e.g., trace file) */
        GTH_EINTERNAL, /**< Internal error */
    } gth_status_t;

    /**
     * Scheduler policy for thread priority management.
     */
    typedef enum
    {
        GTH_SCHED_RR = 0,   /**< Round-robin with equal time quanta */
        GTH_SCHED_PRIORITY, /**< Priority-based preemptive scheduling */
    } gth_sched_policy_t;

    /**
     * Runtime configuration passed to gth_runtime_init(). All fields are
     * copied; the caller may discard the struct after initialization.
     *
     * Defaults are applied for any zero-valued fields that have meaningful
     * non-zero defaults (e.g., stack_size_bytes).
     */
    typedef struct
    {
        size_t stack_size_bytes;        /**< Stack size per thread in bytes */
        gth_sched_policy_t policy;      /**< Scheduler policy */
        uint32_t quantum_us;            /**< Time quantum in microseconds (RR only) */
        uint32_t replay_seed;           /**< Seed for deterministic replay PRNG */
        int enable_deterministic_trace; /**< Enable deterministic trace timestamps */
        int enable_schedule_fuzzing;    /**< Enable schedule fuzzing mode */
    } gth_runtime_config_t;

    /**
     * Thread creation attributes. Pass NULL to gth_thread_create() for
     * default attributes. All zero-initialized fields use built-in defaults.
     *
     * The `name` field is for debugging purposes only. It is stored by the
     * user but never read by the runtime implementation. It may be used in
     * future versions for diagnostic output or trace annotations. Callers
     * should treat it as a convenience tag that has no effect on thread
     * behavior or scheduling.
     */
    typedef struct
    {
        gth_tid_t tid;     /**< Out-only: filled by gth_thread_create() after creation */
        uint32_t priority; /**< Thread priority (higher = more urgent, SCHED_PRIORITY only) */
        const char *name;  /**< Debug-only name tag; never consumed by the runtime */
    } gth_thread_attr_t;

    /**
     * Mutex type. Opaque; all storage is inline (no heap allocation).
     * Must be zero-initialized or initialized via gth_mutex_init() before use.
     */
    typedef struct
    {
        uint64_t _opaque[10];
    } gth_mutex_t;

    /**
     * Counting semaphore type. Opaque; all storage is inline.
     * Must be zero-initialized or initialized via gth_sem_init() before use.
     */
    typedef struct
    {
        uint64_t _opaque[8];
    } gth_sem_t;

    /**
     * Condition variable type. Opaque; all storage is inline.
     * Must be zero-initialized or initialized via gth_cond_init() before use.
     */
    typedef struct
    {
        uint64_t _opaque[8];
    } gth_cond_t;

    /**
     * Thread entry-point function signature. The function receives a single
     * user-provided argument and returns an opaque value retrievable via
     * gth_thread_join().
     */
    typedef void *(*gth_thread_fn)(void *arg);

    /* ========================================================================
     * Runtime lifecycle
     * ======================================================================== */

    /**
     * \brief Initialize the threading runtime with the given configuration.
     *
     * Must be called exactly once before any other gthreads function. Copies
     * the config struct internally; the caller retains ownership.
     *
     * \param[in] config  Runtime configuration. NULL selects built-in defaults.
     *
     * \retval GTH_OK      Runtime initialized successfully.
     * \retval GTH_ESTATE  Runtime is already initialized.
     * \retval GTH_ENOMEM  Memory allocation failed during initialization.
     */
    gth_status_t gth_runtime_init(const gth_runtime_config_t *config);

    /**
     * \brief Shut down the threading runtime and release all resources.
     *
     * Blocks until all threads have finished. After return, the runtime may
     * be re-initialized with gth_runtime_init(). All outstanding TIDs and
     * synchronization object handles become invalid.
     *
     * \retval GTH_OK      Shutdown completed.
     * \retval GTH_ESTATE  Runtime was not initialized.
     */
    gth_status_t gth_runtime_shutdown(void);

    /* ========================================================================
     * Thread management
     * ======================================================================== */

    /**
     * \brief Create a new thread and add it to the ready queue.
     *
     * The new thread begins execution at function \p fn with argument \p arg
     * as soon as the scheduler selects it. If \p attr is non-NULL, its tid
     * field is filled with the assigned TID.
     *
     * \param[out] out_tid  Receives the TID of the newly created thread.
     * \param[in]  attr     Thread attributes, or NULL for defaults.
     * \param[in]  fn       Thread entry-point function. Must not be NULL.
     * \param[in]  arg      Argument forwarded to \p fn.
     *
     * \retval GTH_OK       Thread created successfully.
     * \retval GTH_EINVAL   \p out_tid or \p fn is NULL.
     * \retval GTH_ENOMEM   No free thread slots available.
     */
    gth_status_t gth_thread_create(gth_tid_t *out_tid, const gth_thread_attr_t *attr,
                                   gth_thread_fn fn, void *arg);

    /**
     * \brief Yield the current thread's remaining time quantum to the scheduler.
     *
     * The calling thread moves to the back of the ready queue. If no other
     * thread is ready, the caller resumes immediately.
     *
     * \retval GTH_OK      Yield completed.
     * \retval GTH_ESTATE  Runtime not initialized.
     */
    gth_status_t gth_thread_yield(void);

    /**
     * \brief Block until the specified thread has finished execution.
     *
     * If the target thread has already finished, gth_thread_join() returns
     * immediately. A thread may only be joined once; joining a thread that
     * is already joined returns GTH_ESTATE.
     *
     * \param[in]  tid     TID of the thread to join.
     * \param[out] retval  Receives the thread's return value, or NULL to ignore.
     *
     * \retval GTH_OK       Join completed; *retval set (if non-NULL).
     * \retval GTH_EINVAL   \p tid is 0 (invalid TID).
     * \retval GTH_ESTATE   Thread not found, already joined, or runtime not running.
     */
    gth_status_t gth_thread_join(gth_tid_t tid, void **retval);

    /**
     * \brief Request cancellation of the specified thread.
     *
     * The target thread is marked for cancellation and will terminate at the
     * next scheduling point. If the thread is blocked, it is unblocked first.
     *
     * \param[in] tid  TID of the thread to cancel.
     *
     * \retval GTH_OK       Cancellation requested.
     * \retval GTH_EINVAL   \p tid is 0 (invalid TID).
     * \retval GTH_ESTATE   Thread not found or already finished.
     */
    gth_status_t gth_thread_cancel(gth_tid_t tid);

    /**
     * \brief Return the TID of the calling thread.
     *
     * Returns 0 if called outside a valid thread context (e.g., during
     * runtime initialization).
     *
     * \return Current thread's TID, or 0 if unavailable.
     */
    gth_tid_t gth_thread_self(void);

    /* ========================================================================
     * Mutex
     * ======================================================================== */

    /**
     * \brief Initialize a mutex for use.
     *
     * The mutex must be zero-initialized before calling this function.
     * After initialization it may be locked, try-locked, and unlocked.
     *
     * \param[in,out] m  Pointer to the mutex to initialize.
     *
     * \retval GTH_OK       Mutex initialized.
     * \retval GTH_EINVAL   \p m is NULL.
     */
    gth_status_t gth_mutex_init(gth_mutex_t *m);

    /**
     * \brief Acquire the mutex, blocking if it is already held.
     *
     * If the mutex is held by another thread, the calling thread blocks
     * until the owner releases it. A thread that already holds the mutex
     * must not lock it again (deadlock).
     *
     * \param[in,out] m  Pointer to the mutex.
     *
     * \retval GTH_OK       Mutex acquired.
     * \retval GTH_EINVAL   \p m is NULL.
     * \retval GTH_ESTATE   Runtime not initialized.
     */
    gth_status_t gth_mutex_lock(gth_mutex_t *m);

    /**
     * \brief Attempt to acquire the mutex without blocking.
     *
     * Returns immediately with GTH_OK if the mutex is available, or
     * GTH_EBUSY if it is already held.
     *
     * \param[in,out] m  Pointer to the mutex.
     *
     * \retval GTH_OK      Mutex acquired.
     * \retval GTH_EBUSY  Mutex is held by another thread.
     * \retval GTH_EINVAL \p m is NULL.
     */
    gth_status_t gth_mutex_trylock(gth_mutex_t *m);

    /**
     * \brief Release a held mutex.
     *
     * The calling thread must currently own the mutex. If other threads are
     * blocked waiting on this mutex, one of them is woken.
     *
     * \param[in,out] m  Pointer to the mutex.
     *
     * \retval GTH_OK      Mutex released.
     * \retval GTH_EINVAL \p m is NULL.
     * \retval GTH_ESTATE Caller does not own the mutex.
     */
    gth_status_t gth_mutex_unlock(gth_mutex_t *m);

    /**
     * \brief Destroy a mutex and release associated resources.
     *
     * The mutex must not be locked when destroyed. After destruction the
     * memory may be freed or re-initialized.
     *
     * \param[in,out] m  Pointer to the mutex.
     *
     * \retval GTH_OK      Mutex destroyed.
     * \retval GTH_EINVAL \p m is NULL.
     * \retval GTH_EBUSY  Mutex is currently locked.
     */
    gth_status_t gth_mutex_destroy(gth_mutex_t *m);

    /* ========================================================================
     * Counting semaphore
     * ======================================================================== */

    /**
     * \brief Initialize a counting semaphore with the given initial count.
     *
     * \param[in,out] s             Pointer to the semaphore.
     * \param[in]     initial_count Starting count (must not cause overflow).
     *
     * \retval GTH_OK       Semaphore initialized.
     * \retval GTH_EINVAL   \p s is NULL.
     */
    gth_status_t gth_sem_init(gth_sem_t *s, uint32_t initial_count);

    /**
     * \brief Decrement (acquire) the semaphore, blocking if the count is zero.
     *
     * If the semaphore count is zero, the calling thread blocks until
     * another thread calls gth_sem_post().
     *
     * \param[in,out] s  Pointer to the semaphore.
     *
     * \retval GTH_OK       Semaphore acquired.
     * \retval GTH_EINVAL   \p s is NULL.
     * \retval GTH_ESTATE   Runtime not initialized.
     */
    gth_status_t gth_sem_wait(gth_sem_t *s);

    /**
     * \brief Increment (release) the semaphore, waking a blocked waiter if any.
     *
     * \param[in,out] s  Pointer to the semaphore.
     *
     * \retval GTH_OK      Semaphore released.
     * \retval GTH_EINVAL \p s is NULL.
     */
    gth_status_t gth_sem_post(gth_sem_t *s);

    /**
     * \brief Destroy a semaphore and release associated resources.
     *
     * The semaphore must not have waiters when destroyed.
     *
     * \param[in,out] s  Pointer to the semaphore.
     *
     * \retval GTH_OK      Semaphore destroyed.
     * \retval GTH_EINVAL \p s is NULL.
     * \retval GTH_EBUSY  Threads are waiting on this semaphore.
     */
    gth_status_t gth_sem_destroy(gth_sem_t *s);

    /* ========================================================================
     * Condition variable
     * ======================================================================== */

    /**
     * \brief Initialize a condition variable for use.
     *
     * \param[in,out] c  Pointer to the condition variable.
     *
     * \retval GTH_OK       Condition variable initialized.
     * \retval GTH_EINVAL   \p c is NULL.
     */
    gth_status_t gth_cond_init(gth_cond_t *c);

    /**
     * \brief Atomically release the mutex and block on the condition variable.
     *
     * The calling thread releases \p m and suspends until another thread
     * calls gth_cond_signal() or gth_cond_broadcast() on \p c, at which
     * point the mutex is re-acquired before returning.
     *
     * \param[in,out] c  Pointer to the condition variable.
     * \param[in,out] m  Pointer to the associated mutex.
     *
     * \retval GTH_OK       Awoken and mutex re-acquired.
     * \retval GTH_EINVAL   \p c or \p m is NULL.
     * \retval GTH_ESTATE   Runtime not initialized.
     */
    gth_status_t gth_cond_wait(gth_cond_t *c, gth_mutex_t *m);

    /**
     * \brief Wake exactly one thread blocked on the condition variable.
     *
     * If no thread is currently waiting, the signal is lost (no state is
     * retained).
     *
     * \param[in,out] c  Pointer to the condition variable.
     *
     * \retval GTH_OK      At most one waiter woken.
     * \retval GTH_EINVAL \p c is NULL.
     */
    gth_status_t gth_cond_signal(gth_cond_t *c);

    /**
     * \brief Wake all threads blocked on the condition variable.
     *
     * \param[in,out] c  Pointer to the condition variable.
     *
     * \retval GTH_OK      All waiters woken.
     * \retval GTH_EINVAL \p c is NULL.
     */
    gth_status_t gth_cond_broadcast(gth_cond_t *c);

    /**
     * \brief Destroy a condition variable and release associated resources.
     *
     * The condition variable must have no blocked waiters when destroyed.
     *
     * \param[in,out] c  Pointer to the condition variable.
     *
     * \retval GTH_OK      Condition variable destroyed.
     * \retval GTH_EINVAL \p c is NULL.
     * \retval GTH_EBUSY  Threads are waiting on this condition variable.
     */
    gth_status_t gth_cond_destroy(gth_cond_t *c);

    /* ========================================================================
     * Trace and replay
     * ======================================================================== */

    /**
     * \brief Start recording a deterministic execution trace to disk.
     *
     * All scheduling decisions and synchronization events are captured to
     * the file at \p trace_path. Only one trace may be active at a time.
     *
     * \param[in] trace_path  Filesystem path for the trace output file.
     *
     * \retval GTH_OK        Trace recording started.
     * \retval GTH_EINVAL   \p trace_path is NULL or empty.
     * \retval GTH_ESTATE   Runtime not initialized or trace already active.
     * \retval GTH_ENOMEM   Memory allocation for trace buffer failed.
     */
    gth_status_t gth_trace_start(const char *trace_path);

    /**
     * \brief Stop recording and flush the trace to disk.
     *
     * Flushes any buffered trace events and closes the trace file. If no
     * trace is active, this is a no-op returning GTH_OK.
     *
     * \retval GTH_OK      Trace stopped and flushed.
     * \retval GTH_ESTATE  Runtime not initialized.
     */
    gth_status_t gth_trace_stop(void);

    /**
     * \brief Replay a previously recorded execution trace.
     *
     * The scheduler drives all thread selections from the trace file rather
     * than the normal scheduling policy. Returns immediately after setting
     * up replay state; the actual replay occurs as the runtime executes.
     *
     * \param[in] trace_path  Filesystem path to the trace file.
     *
     * \retval GTH_OK        Replay initialized.
     * \retval GTH_EINVAL   \p trace_path is NULL or empty.
     * \retval GTH_ESTATE   Runtime not initialized or replay already active.
     * \retval GTH_ENOTFOUND Trace file could not be opened.
     */
    gth_status_t gth_replay_from(const char *trace_path);

    /* ========================================================================
     * Runtime statistics
     * ======================================================================== */

    /**
     * Aggregate runtime statistics snapshot.
     */
    typedef struct
    {
        uint64_t context_switches; /**< Total context switches since init */
        uint64_t runnable_threads; /**< Threads currently in ready state */
        uint64_t blocked_threads;  /**< Threads currently blocked */
    } gth_runtime_stats_t;

    /**
     * \brief Retrieve aggregate runtime statistics.
     *
     * \param[out] out_stats  Receives the current statistics snapshot.
     *
     * \retval GTH_OK       Stats copied successfully.
     * \retval GTH_EINVAL  \p out_stats is NULL.
     * \retval GTH_ESTATE  Runtime not initialized.
     */
    gth_status_t gth_runtime_get_stats(gth_runtime_stats_t *out_stats);

    /* ========================================================================
     * Fuzz subsystem (schedule perturbation)
     * ======================================================================== */

    /**
     * Statistics for the schedule fuzzing subsystem.
     */
    typedef struct
    {
        uint64_t decision_count;    /**< Scheduling decisions made since fuzz init */
        uint32_t perturbation_rate; /**< Current perturbation rate (0-100, percent) */
    } gth_fuzz_stats_t;

    /**
     * \brief Retrieve statistics from the schedule fuzzing subsystem.
     *
     * The fuzz subsystem must be active (gth_runtime_config_t.enable_schedule_fuzzing
     * was set and the runtime is running).
     *
     * \param[out] out_stats  Receives the fuzz statistics snapshot.
     *
     * \retval GTH_OK       Stats copied successfully.
     * \retval GTH_EINVAL  \p out_stats is NULL.
     * \retval GTH_ESTATE  Runtime not initialized or fuzzing not active.
     */
    gth_status_t gth_fuzz_get_stats(gth_fuzz_stats_t *out_stats);

    /**
     * \brief Set the perturbation rate for the schedule fuzzer.
     *
     * The rate is a percentage from 0 (disabled) to 100 (perturb every
     * decision after warmup). Values are clamped to [0, 100].
     *
     * \param[in] rate  New perturbation rate in percent.
     *
     * \retval GTH_OK       Rate updated.
     * \retval GTH_ESTATE  Runtime not initialized or fuzzing not active.
     * \retval GTH_EINVAL  Rate out of range.
     */
    gth_status_t gth_fuzz_set_rate(uint32_t rate);

    /* ========================================================================
     * Replay subsystem statistics
     * ======================================================================== */

    /**
     * Statistics for the deterministic replay subsystem.
     */
    typedef struct
    {
        uint64_t event_count; /**< Total events in the loaded trace */
        uint64_t current_idx; /**< Index of the next event to consume */
        int diverged;         /**< Non-zero if execution diverged from trace */
    } gth_replay_stats_t;

    /**
     * \brief Retrieve statistics from the deterministic replay subsystem.
     *
     * The replay subsystem must be active (gth_replay_from() was called
     * and the runtime has not yet shut down).
     *
     * \param[out] out_stats  Receives the replay statistics snapshot.
     *
     * \retval GTH_OK       Stats copied successfully.
     * \retval GTH_EINVAL  \p out_stats is NULL.
     * \retval GTH_ESTATE  Runtime not initialized or replay not active.
     */
    gth_status_t gth_replay_get_stats(gth_replay_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif
