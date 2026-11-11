/*
 * x86_64 context switch for gthreads: gth_ctx_swap (save/restore),
 * gth_ctx_make (first-time init). FPU state: XSAVEOPT if CPU+OS allow,
 * else FXSAVE; chosen once via gth_fp_xsave_kind / gth_fp_rfbm (context.c).
 * endbr64 keeps indirect jumps IBT-safe; NOP elsewhere.
 */

    .text

    .extern gth_fp_xsave_kind
    .extern gth_fp_rfbm

/* ------------------------------------------------------------------ */
/* gth_ctx_swap(gth_ctx_t *from, gth_ctx_t *to)                       */
/*   from = %rdi, to = %rsi                                           */
/* ------------------------------------------------------------------ */
    .globl  gth_ctx_swap
    .type   gth_ctx_swap, @function
gth_ctx_swap:
    endbr64
    .cfi_startproc
    .cfi_undefined %rbx
    .cfi_undefined %rbp
    .cfi_undefined %r12
    .cfi_undefined %r13
    .cfi_undefined %r14
    .cfi_undefined %r15

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

    /* Save FPU state if fp_state_area is non-NULL */
    movq   64(%rdi), %r11       /* r11 = from->fp_state_area (volatile) */
    testq  %r11, %r11
    jz     .Lskip_fpsave
    movl   gth_fp_xsave_kind(%rip), %ecx
    testl  %ecx, %ecx
    jz     .Lswap_fpsave
    movl   gth_fp_rfbm(%rip), %eax
    movl   gth_fp_rfbm + 4(%rip), %edx
    xsaveopt64 (%r11)
    jmp    .Lskip_fpsave
.Lswap_fpsave:
    fxsave (%r11)
.Lskip_fpsave:

    /* Restore FPU state if fp_state_area is non-NULL */
    movq   64(%rsi), %r11       /* r11 = to->fp_state_area (volatile) */
    testq  %r11, %r11
    jz     .Lskip_fxrstor
    movl   gth_fp_xsave_kind(%rip), %ecx
    testl  %ecx, %ecx
    jz     .Lswap_fxrstor
    movl   gth_fp_rfbm(%rip), %eax
    movl   gth_fp_rfbm + 4(%rip), %edx
    xrstor64 (%r11)
    jmp    .Lskip_fxrstor
.Lswap_fxrstor:
    fxrstor (%r11)
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
    endbr64
    ret

    .cfi_endproc
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
    endbr64
    .cfi_startproc

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

    /* Seed FP state with creator's value: all-zero state has invalid FCW and faults on restore. */
    movq    64(%rdi), %r11      /* r11 = fp_state_area pointer (volatile) */
    testq   %r11, %r11
    jz      .Lmake_done
    movl    gth_fp_xsave_kind(%rip), %ecx
    testl   %ecx, %ecx
    jz      .Lmake_fpsave
    movl    gth_fp_rfbm(%rip), %eax
    movl    gth_fp_rfbm + 4(%rip), %edx
    xsaveopt64 (%r11)
    jmp     .Lmake_done
.Lmake_fpsave:
    fxsave  (%r11)
.Lmake_done:
    ret

    .cfi_endproc
    .size   gth_ctx_make, . - gth_ctx_make

/* ------------------------------------------------------------------ */
/* gth_ctx_trampoline - first-entry trampoline for new contexts        */
/*                                                                    */
/* Entered via jmpq from gth_ctx_swap on first context switch.        */
/* RSP was set by gth_ctx_make to stack_top - 16.                     */
/* [RSP+0] = entry function pointer                                   */
/* [RSP+8] = slot_index (64-bit size_t)                               */
/* ------------------------------------------------------------------ */
    .globl  gth_ctx_trampoline
    .type   gth_ctx_trampoline, @function
gth_ctx_trampoline:
    endbr64
    movq     (%rsp), %rax       /* rax = entry function pointer */
    movq    8(%rsp), %rdi       /* rdi = slot_index (1st C arg) */
    callq   *%rax               /* call entry(slot_index) */
    ud2                         /* entry must not return; trap if it does */

    .size   gth_ctx_trampoline, . - gth_ctx_trampoline

    /* Mark stack as non-executable to avoid linker warning */
    .section .note.GNU-stack,"",@progbits
