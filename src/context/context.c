#include "gthreads/gthreads.h"

/*
 * Context backend placeholder.
 *
 * The M2 implementation for now uses the stack allocation helpers in
 * `src/context/stack.c` while the actual low-level context switching backend
 * is introduced later.
 */
gth_status_t gth_context_backend_placeholder(void)
{
    return GTH_OK;
}
