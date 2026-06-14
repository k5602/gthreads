/*
 * Scheduler Module
 *
 * Cooperative scheduler with mode-aware dispatch:
 *   - NORMAL: Standard RR or Priority scheduling
 *   - RECORD: Standard scheduling + trace recording
 *   - REPLAY: Trace-driven deterministic scheduling
 *   - FUZZ: Perturbed scheduling with random variations
 */

#include <stddef.h>

#include "runtime_state.h"

/*
 * Standard ready thread selection based on policy (RR or Priority)
 */
static gth_thread_record_t *gth_scheduler_pick_ready_thread(gth_runtime_state_t *state)
{
    gth_thread_record_t *best = NULL;

    if (state == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        gth_thread_record_t *candidate = &state->threads[i];

        if (candidate->state != GTH_THREAD_READY)
        {
            continue;
        }

        if (best == NULL)
        {
            best = candidate;
            continue;
        }

        if (state->config.policy == GTH_SCHED_PRIORITY)
        {
            if (candidate->priority > best->priority)
            {
                best = candidate;
                continue;
            }

            if (candidate->priority == best->priority && candidate->tid < best->tid)
            {
                best = candidate;
                continue;
            }
        }
        else
        {
            if (candidate->tid < best->tid)
            {
                best = candidate;
            }
        }
    }

    return best;
}

/*
 * Mode-aware scheduler selection.
 *
 * Scheduling modes:
 *   - NORMAL: Standard policy-based selection
 *   - RECORD: Standard selection (recording happens in run_next)
 *   - REPLAY: Use trace-driven selection from replay module
 *   - FUZZ: Perturb the standard selection randomly
 */
gth_thread_record_t *gth_scheduler_pick_ready_thread_mode(gth_runtime_state_t *state)
{
    gth_thread_record_t *normal_choice = NULL;

    if (state == NULL)
    {
        return NULL;
    }

    /*
     * REPLAY mode: Let the replay engine drive scheduling
     * The trace tells us exactly which thread should run next.
     */
    if (state->mode == GTH_MODE_REPLAY && state->replay != NULL)
    {
        return gth_replay_next_thread(state);
    }

    /*
     * NORMAL and RECORD modes: Use standard policy selection
     */
    normal_choice = gth_scheduler_pick_ready_thread(state);

    /*
     * FUZZ mode: Potentially perturb the selection
     * This introduces controlled randomness to find race conditions.
     */
    if (state->mode == GTH_MODE_FUZZ && state->fuzz != NULL)
    {
        return gth_fuzz_pick_thread(state, normal_choice);
    }

    return normal_choice;
}

/*
 * Switch between normal, record, replay, and fuzz modes.
 */
gth_status_t gth_scheduler_set_mode(gth_scheduler_mode_t mode)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    /* Cleanup previous mode if needed */
    if (state->mode == GTH_MODE_RECORD && state->trace != NULL)
    {
        gth_trace_cleanup();
    }

    if (state->mode == GTH_MODE_REPLAY && state->replay != NULL)
    {
        gth_replay_cleanup();
    }

    if (state->mode == GTH_MODE_FUZZ && state->fuzz != NULL)
    {
        gth_fuzz_cleanup();
    }

    state->mode = mode;

    /* Initialize new mode if needed */
    if (mode == GTH_MODE_RECORD)
    {
        gth_status_t status = gth_trace_init();
        if (status != GTH_OK && status != GTH_ESTATE)
        {
            state->mode = GTH_MODE_NORMAL;
            return status;
        }
    }

    if (mode == GTH_MODE_FUZZ)
    {
        /* Initialize fuzz with seed from config */
        uint64_t seed = state->config.replay_seed;
        if (seed == 0)
        {
            seed = 123456789ULL; /* Default seed if none specified */
        }

        gth_status_t status = gth_fuzz_init(seed);
        if (status != GTH_OK)
        {
            state->mode = GTH_MODE_NORMAL;
            return status;
        }
    }

    return GTH_OK;
}

gth_status_t gth_scheduler_run_next(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_thread_record_t *thread = NULL;
    gth_tid_t previous_tid = 0U;

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    /* Use mode-aware thread selection */
    thread = gth_scheduler_pick_ready_thread_mode(state);
    if (thread == NULL)
    {
        return GTH_ENOTFOUND;
    }

    previous_tid = state->current_tid;

    /* Record context switch before it happens (RECORD mode) */
    if (state->mode == GTH_MODE_RECORD || (state->trace != NULL && state->trace->active))
    {
        gth_trace_context_switch(previous_tid, thread->tid);
    }

    thread->state = GTH_THREAD_RUNNING;
    state->current_tid = thread->tid;
    state->context_switches += 1U;

    if (state->runnable_threads > 0U)
    {
        state->runnable_threads -= 1U;
    }

    gth_ctx_swap(&state->scheduler_ctx, &thread->ctx);

    /* Thread has yielded/blocked/exited and returned to scheduler */
    state->current_tid = previous_tid;

    return GTH_OK;
}

gth_status_t gth_scheduler_run_until(gth_tid_t tid)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_thread_record_t *target = NULL;

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    target = gth_runtime_find_thread(state, tid);
    if (target == NULL)
    {
        return GTH_ENOTFOUND;
    }

    while (!gth_thread_is_terminal(target->state))
    {
        gth_status_t status = gth_scheduler_run_next();
        if (status == GTH_ENOTFOUND)
        {
            return GTH_EBUSY;
        }
        if (status != GTH_OK)
        {
            return status;
        }
    }

    return GTH_OK;
}
