/*
 * x86_64 assembly context switching for gthreads.
 *
 * Provides gth_ctx_swap (save/restore) and gth_ctx_make (first-time init).
 * Follows the libaco pattern: save callee-saved GP registers + FPU state.
 *
 * SysV AMD64 ABI callee-saved registers: RBX, RBP, R12-R15.
 * RSP and RIP are saved explicitly.
 * FPU/SSE state (XMM6-XMM15, MXCSR, x87 FCW) saved via fxsave/fxrstor.
 */

    .text

/* ------------------------------------------------------------------ */
/* gth_ctx_swap(gth_ctx_t *from, gth_ctx_t *to)                       */
/*   from = %rdi, to = %rsi                                           */
/* ------------------------------------------------------------------ */
    .globl  gth_ctx_swap
    .type   gth_ctx_swap, @function
gth_ctx_swap:
    /* Save callee-saved GP registers into from->regs */
    movq    %rbx,    (%rdi)     /* regs[0] = RBX */
    movq    %rbp,   8(%rdi)     /* regs[1] = RBP */
    movq    %r12,  16(%rdi)     /* regs[2] = R12 */
    movq    %r13,  24(%rdi)     /* regs[3] = R13 */
    movq    %r14,  32(%rdi)     /* regs[4] = R14 */
    movq    %r15,  40(%rdi)     /* regs[5] = R15 */
    movq    %rsp,  48(%rdi)     /* regs[6] = RSP */

    /* Save resume address (address of label 1) as RIP */
    leaq    1f(%rip), %rax
    movq    %rax,  56(%rdi)     /* regs[7] = RIP */

    /* Save FPU/SSE state if fxsave_area is non-NULL */
    movq   64(%rdi), %rax       /* rax = from->fxsave_area */
    testq  %rax, %rax
    jz     .Lskip_fxsave
    fxsave (%rax)
.Lskip_fxsave:

    /* Restore FPU/SSE state if fxsave_area is non-NULL */
    movq   64(%rsi), %rax       /* rax = to->fxsave_area */
    testq  %rax, %rax
    jz     .Lskip_fxrstor
    fxrstor (%rax)
.Lskip_fxrstor:

    /* Restore callee-saved GP registers from to->regs */
    movq     (%rsi), %rbx      /* RBX */
    movq    8(%rsi), %rbp      /* RBP */
    movq   16(%rsi), %r12      /* R12 */
    movq   24(%rsi), %r13      /* R13 */
    movq   32(%rsi), %r14      /* R14 */
    movq   40(%rsi), %r15      /* R15 */
    movq   48(%rsi), %rsp      /* RSP */

    /* Jump to restored RIP */
    jmpq   *56(%rsi)

    /* Resume point: when we are swapped back to, execution lands here */
1:
    ret

    .size   gth_ctx_swap, . - gth_ctx_swap

/* ------------------------------------------------------------------ */
/* gth_ctx_make(gth_ctx_t *ctx, void *stack_top,                      */
/*              void (*entry)(size_t), size_t slot_index)             */
/*   ctx        = %rdi                                                */
/*   stack_top  = %rsi                                                */
/*   entry      = %rdx                                                */
/*   slot_index = %rcx                                                */
/* ------------------------------------------------------------------ */
    .globl  gth_ctx_make
    .type   gth_ctx_make, @function
gth_ctx_make:
    /* Zero callee-saved GP registers [0..5] */
    xorq    %rax, %rax
    movq    %rax,    (%rdi)     /* regs[0] = RBX = 0 */
    movq    %rax,   8(%rdi)     /* regs[1] = RBP = 0 */
    movq    %rax,  16(%rdi)     /* regs[2] = R12 = 0 */
    movq    %rax,  24(%rdi)     /* regs[3] = R13 = 0 */
    movq    %rax,  32(%rdi)     /* regs[4] = R14 = 0 */
    movq    %rax,  40(%rdi)     /* regs[5] = R15 = 0 */

    /* Set RSP = stack_top - 16 (16-byte aligned, room for trampoline args) */
    leaq   -16(%rsi), %rax
    movq    %rax, 48(%rdi)      /* regs[6] = RSP */

    /* Store entry args on the stack at [RSP] and [RSP+8] */
    movq    %rdx, (%rax)        /* [RSP+0] = entry function pointer */
    movq    %rcx, 8(%rax)       /* [RSP+8] = slot_index (size_t) */

    /* Set RIP = gth_ctx_trampoline */
    leaq    gth_ctx_trampoline(%rip), %rax
    movq    %rax, 56(%rdi)      /* regs[7] = RIP = trampoline */

    /* Save current FPU state as the initial state for this context.
     * All-zero fxsave data has an invalid x87 FCW which causes #GP
     * on fxrstor. Saving the parent's FPU state gives a valid initial
     * configuration (default FCW, MXCSR, etc.). */
    movq    64(%rdi), %rax      /* rax = fxsave_area pointer */
    testq   %rax, %rax
    jz      .Lmake_done
    fxsave  (%rax)
.Lmake_done:
    ret

    .size   gth_ctx_make, . - gth_ctx_make

/* ------------------------------------------------------------------ */
/* gth_ctx_trampoline - first-entry trampoline for new contexts        */
/*                                                                    */
/* Entered via jmpq from gth_ctx_swap on first context switch.        */
/* RSP was set by gth_ctx_make to stack_top - 16.                     */
/* [RSP+0] = entry function pointer                                   */
/* [RSP+8] = slot_index (sign-extended to 64 bits)                    */
/* ------------------------------------------------------------------ */
    .globl  gth_ctx_trampoline
    .type   gth_ctx_trampoline, @function
gth_ctx_trampoline:
    movq     (%rsp), %rax       /* rax = entry function pointer */
    movq    8(%rsp), %rdi       /* rdi = slot_index (1st C arg) */
    callq   *%rax               /* call entry(slot_index) */
    ud2                         /* entry must not return; trap if it does */

    .size   gth_ctx_trampoline, . - gth_ctx_trampoline

    /* Mark stack as non-executable to avoid linker warning */
    .section .note.GNU-stack,"",@progbits
