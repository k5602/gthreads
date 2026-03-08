#include <stddef.h>
#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

static gth_runtime_state_t g_state;

gth_runtime_state_t *gth_runtime_state(void)
{
    return &g_state;
}

gth_thread_record_t *gth_runtime_find_thread(gth_runtime_state_t *state, gth_tid_t tid)
{
    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state != GTH_THREAD_EMPTY && state->threads[i].tid == tid)
        {
            return &state->threads[i];
        }
    }
    return NULL;
}

gth_thread_record_t *gth_runtime_alloc_thread_slot(gth_runtime_state_t *state)
{
    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state == GTH_THREAD_EMPTY)
        {
            return &state->threads[i];
        }
    }
    return NULL;
}

int gth_thread_is_terminal(gth_thread_state_t state)
{
    return (state == GTH_THREAD_DONE || state == GTH_THREAD_CANCELED);
}

static int gth_config_is_valid(const gth_runtime_config_t *config)
{
    if (config == NULL)
    {
        return 0;
    }
    if (config->stack_size_bytes < 16U * 1024U)
    {
        return 0;
    }
    if (config->quantum_us == 0U)
    {
        return 0;
    }
    if (config->policy != GTH_SCHED_RR && config->policy != GTH_SCHED_PRIORITY)
    {
        return 0;
    }
    return 1;
}

gth_status_t gth_runtime_init(const gth_runtime_config_t *config)
{
    if (!gth_config_is_valid(config))
    {
        return GTH_EINVAL;
    }
    if (g_state.initialized)
    {
        return GTH_ESTATE;
    }

    memset(&g_state, 0, sizeof(g_state));
    g_state.initialized = 1;
    g_state.config = *config;
    g_state.next_tid = 1;
    g_state.current_tid = 0;
    g_state.trace_enabled = 0;
    return GTH_OK;
}

gth_status_t gth_runtime_shutdown(void)
{
    if (!g_state.initialized)
    {
        return GTH_ESTATE;
    }
    memset(&g_state, 0, sizeof(g_state));
    return GTH_OK;
}

gth_status_t gth_runtime_get_stats(gth_runtime_stats_t *out_stats)
{
    if (out_stats == NULL)
    {
        return GTH_EINVAL;
    }
    if (!g_state.initialized)
    {
        return GTH_ESTATE;
    }
    out_stats->context_switches = g_state.context_switches;
    out_stats->runnable_threads = g_state.runnable_threads;
    out_stats->blocked_threads = g_state.blocked_threads;
    return GTH_OK;
}
