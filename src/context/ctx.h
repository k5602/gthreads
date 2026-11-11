#ifndef GTHREADS_CTX_H
#define GTHREADS_CTX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Custom context for cooperative switching on x86_64.
     * Layout: 8 callee-saved GP registers + pointer to the FPU area.
     *
     * regs layout:
     *   [0] RBX  [1] RBP  [2] R12  [3] R13
     *   [4] R14  [5] R15  [6] RSP  [7] RIP
     *
     * fp_state_area is NULL for FPU-free contexts (scheduler), else points at
     * fp_state_buf. buf holds a full XSAVE area when CPU+OS support it, else a
     * legacy 512-byte FXSAVE area. >4096-byte XSAVE areas (AMX) force FXSAVE.
     */
    typedef struct
    {
        void *regs[8];
        void *fp_state_area;
        _Alignas(64) uint8_t fp_state_buf[4096];
    } gth_ctx_t;

    /*
     * Save current context into `from`, restore from `to`.
     * `to` must have been initialized by gth_ctx_make or a previous gth_ctx_swap.
     * Returns when resumed; does not return when switching to a fresh context.
     */
    void gth_ctx_swap(gth_ctx_t *from, gth_ctx_t *to);

    /*
     * Initialize a new context for first-time entry.
     *
     * ctx:        context to initialize
     * stack_top:  top of usable stack (16-byte aligned)
     * entry:      function to call on first entry (receives slot_index as arg)
     * slot_index: argument passed to entry function
     *
     * After gth_ctx_swap to this context, execution begins at entry(slot_index).
     * The entry function must not return.
     */
    void gth_ctx_make(gth_ctx_t *ctx, void *stack_top, void (*entry)(size_t), size_t slot_index);

#ifdef __cplusplus
}
#endif

#endif /* GTHREADS_CTX_H */
