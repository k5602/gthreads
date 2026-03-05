#include <stdio.h>
#include <string.h>

#include "gthreads/gthreads.h"
#include "runtime_state.h"

static FILE *g_trace_file = NULL;

gth_status_t gth_trace_start(const char *trace_path)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }
    if (trace_path == NULL || trace_path[0] == '\0')
    {
        return GTH_EINVAL;
    }
    if (g_trace_file != NULL)
    {
        return GTH_ESTATE;
    }

    g_trace_file = fopen(trace_path, "wb");
    if (g_trace_file == NULL)
    {
        return GTH_EINTERNAL;
    }

    static const char header[] = "GTHRACE:1\n";
    if (fwrite(header, 1U, sizeof(header) - 1U, g_trace_file) != (sizeof(header) - 1U))
    {
        fclose(g_trace_file);
        g_trace_file = NULL;
        return GTH_EINTERNAL;
    }

    state->trace_enabled = 1;
    return GTH_OK;
}

gth_status_t gth_trace_stop(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }
    if (g_trace_file == NULL)
    {
        return GTH_ESTATE;
    }

    fclose(g_trace_file);
    g_trace_file = NULL;
    state->trace_enabled = 0;
    return GTH_OK;
}

gth_status_t gth_replay_from(const char *trace_path)
{
    gth_runtime_state_t *state = gth_runtime_state();
    if (!state->initialized)
    {
        return GTH_ESTATE;
    }
    if (trace_path == NULL || trace_path[0] == '\0')
    {
        return GTH_EINVAL;
    }

    FILE *fp = fopen(trace_path, "rb");
    if (fp == NULL)
    {
        return GTH_ENOTFOUND;
    }

    char header[10] = {0};
    size_t bytes = fread(header, 1U, sizeof(header) - 1U, fp);
    fclose(fp);
    if (bytes == 0U || strncmp(header, "GTHRACE:1", 9U) != 0)
    {
        return GTH_ESTATE;
    }
    return GTH_OK;
}
