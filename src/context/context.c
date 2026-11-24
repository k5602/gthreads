#include <string.h>

#include "runtime_state.h"

/* FP save path: XSAVE when CPU+OS support it, FXSAVE otherwise. */
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))

uint32_t gth_fp_xsave_kind = 0; /* 0 = fxsave, 1 = xsave */
uint64_t gth_fp_rfbm = 0;       /* EDX:EAX RFBM mask for the asm */

#define GTH_CPUID1_ECX_XSAVE (3u << 26) /* XSAVE + OSXSAVE */

static uint64_t gth_xgetbv0(void)
{
    uint32_t lo;
    uint32_t hi;
    __asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static void gth_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t out[4])
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    __asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));
    out[0] = a;
    out[1] = b;
    out[2] = c;
    out[3] = d;
}

gth_status_t gth_fp_detect(void)
{
    uint32_t out[4];

    gth_fp_xsave_kind = 0U;
    gth_fp_rfbm = 0U;

    gth_cpuid(1, 0, out);
    if ((out[2] & GTH_CPUID1_ECX_XSAVE) != GTH_CPUID1_ECX_XSAVE)
    {
        return GTH_OK; /* no XSAVE hardware or no OS support: FXSAVE */
    }

    gth_cpuid(0x0D, 0, out);
    const uint64_t supported = ((uint64_t)out[3] << 32) | (uint64_t)out[0];
    const uint64_t rfbm = supported & gth_xgetbv0();

    uint64_t size = 576U; /* legacy 512 + XSAVE header 64 */
    for (uint32_t i = 2; i < 64U; ++i)
    {
        if ((rfbm & (1ULL << i)) == 0ULL)
        {
            continue;
        }
        gth_cpuid(0x0D, i, out);
        const uint64_t end = (uint64_t)out[1] + (uint64_t)out[0];
        if (out[0] != 0U && end > size)
        {
            size = end;
        }
    }

    if (rfbm == 0ULL)
    {
        return GTH_OK;
    }
    if (size > sizeof(((gth_ctx_t *)0)->fp_state_buf))
    {
        return GTH_EINTERNAL; /* enabled state does not fit: no silent FXSAVE fallback */
    }

    gth_fp_rfbm = rfbm;
    gth_fp_xsave_kind = 1U;
    return GTH_OK;
}

#else

gth_status_t gth_fp_detect(void)
{
    return GTH_OK;
}

#endif

gth_status_t gth_context_init_thread(gth_ctx_t *ctx, const gth_stack_allocation_t *stack,
                                     size_t slot_index)
{
    if (ctx == NULL || stack == NULL)
    {
        return GTH_EINVAL;
    }

    ctx->fp_state_area = ctx->fp_state_buf;

    /*
     * stack_top is page-aligned (set by gth_stack_allocate), so it is
     * 16-byte aligned. gth_ctx_make will subtract 16 for trampoline
     * args, keeping 16-byte alignment.
     */
    gth_ctx_make(ctx, stack->stack_top, gth_context_thread_trampoline, slot_index);

    return GTH_OK;
}

void gth_context_destroy(gth_ctx_t *ctx)
{
    if (ctx != NULL)
    {
        ctx->fp_state_area = NULL;
    }
}

void gth_context_thread_trampoline(size_t slot_index_arg)
{
    gth_runtime_state_t *state = gth_runtime_state();
    size_t idx = slot_index_arg;
    gth_thread_record_t *thread = &state->threads[idx];

    void *result = thread->fn(thread->arg);

    if (thread->state != GTH_THREAD_CANCELED)
    {
        thread->retval = result;
        thread->state = GTH_THREAD_DONE;
    }
    else if (thread->state == GTH_THREAD_CANCELED)
    {
        thread->retval = NULL;
    }

    gth_trace_thread_exit(thread->tid, thread->retval);

    state->current_tid = 0U;

    /*
     * Thread is done; switch back to the scheduler. The current thread's
     * context is discarded (it will never be resumed), so we pass a
     * zeroed dummy as the `from` context. gth_ctx_swap checks for NULL
     * fp_state_area before saving, so this is safe.
     */
    gth_ctx_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.fp_state_area = NULL;
    gth_ctx_swap(&dummy, &state->scheduler_ctx);
}
