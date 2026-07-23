# gthreads Architecture

A deterministic user-space threading runtime in C17 for Linux x86_64.

## Overview

gthreads implements cooperative green threads entirely in user space. The runtime manages its own stacks, context switching, scheduling, synchronization primitives, and a trace/replay subsystem for deterministic reproduction of concurrency bugs.

## Module Map

```
gthreads/
  include/gthreads/         Public API (gthreads.h)
  src/
    runtime/                Runtime lifecycle, global state
    context/                Context save/restore (x86_64 ASM + C wrappers)
    sched/                  Scheduler: RR and priority-based
    thread/                 Thread creation, join, cancel, yield
    sync/                   Mutex, semaphore, condition variable
    trace/                  Trace recording, binary format, replay, fuzz
    internal/               Internal headers (runtime_state.h)
  examples/                 Example programs
  tests/                    CMocka unit tests
  docs/                     Documentation
```

## Layer Diagram

```
+------------------------------------------------------------------+
|                     Application / Examples                        |
+------------------------------------------------------------------+
|                       Public API (gthreads.h)                     |
+------------------------------------------------------------------+
|    Thread Mgmt   |  Scheduler  |   Sync Primitives  |   Trace    |
|   (thread.c)     | (scheduler) | (mutex/sem/cond.c)  |  (trace/)  |
+------------------------------------------------------------------+
|              Context Switching  (context.c + ctx_x86_64.S)       |
+------------------------------------------------------------------+
|              Stack Management   (stack.c)                        |
+------------------------------------------------------------------+
|              Runtime State      (runtime.c, runtime_state.h)     |
+------------------------------------------------------------------+
|              OS: mmap, mprotect, clock_gettime                   |
+------------------------------------------------------------------+
```

Each layer depends only on the layers below it. The public API layer is the only external interface; all internal functions use the `gth_` prefix but are not exported in the public header.

## Core Data Structures

### gth_runtime_state_t

The single global runtime state (file-scope static in `runtime.c`). Holds all mutable state for the lifetime of the process.

```c
typedef struct {
    int initialized;
    int shutting_down;
    gth_runtime_config_t config;
    gth_tid_t next_tid;          // Monotonically increasing, starts at 1
    gth_tid_t current_tid;       // TID of the currently running thread
    uint64_t context_switches;   // Total context switches since init
    uint32_t runnable_threads;   // Count of READY threads
    uint32_t blocked_threads;    // Count of BLOCKED threads
    uint32_t finished_threads;   // Count of DONE threads
    size_t last_rr_slot;         // Last slot index picked by Round-Robin scheduler
    gth_ctx_t scheduler_ctx;     // Scheduler's own context (FPU-free)
    gth_thread_record_t threads[GTH_MAX_THREADS]; // Thread slot table

    gth_scheduler_mode_t mode;   // NORMAL | RECORD | REPLAY | FUZZ
    gth_trace_state_t *trace;    // Active trace (NULL if not recording)
    gth_replay_state_t *replay;  // Active replay (NULL if not replaying)
    gth_fuzz_state_t *fuzz;      // Active fuzz state (NULL if not fuzzing)
} gth_runtime_state_t;
```

### gth_thread_record_t

One slot per thread, stored in a fixed-size array (128 slots max). The slot index is used as a stable internal identifier across context switches.

```c
typedef struct {
    gth_tid_t tid;
    gth_thread_fn fn;             // Entry point
    void *arg;                    // User argument
    void *retval;                 // Return value after DONE
    uint32_t priority;
    gth_thread_state_t state;     // EMPTY|READY|RUNNING|BLOCKED|DONE|CANCELED
    gth_ctx_t ctx;                // Saved register context
    gth_stack_allocation_t stack; // Guard-page protected stack
    size_t slot_index;            // Stable index into threads[] array
} gth_thread_record_t;
```

### gth_ctx_t

CPU register save area for cooperative context switching on x86_64. Contains 8 callee-saved registers (RBX, RBP, R12-R15, RSP, RIP) plus an embedded 512-byte `_Alignas(16)` FXSAVE buffer for SSE/x87 state (eliminating dynamic heap allocations and FPU memory leaks).

```c
typedef struct {
    void *regs[8];                       // [0]=RBX [1]=RBP [2]=R12 [3]=R13
                                         // [4]=R14 [5]=R15 [6]=RSP [7]=RIP
    void *fxsave_area;                   // Pointer to embedded fxsave_buf
    _Alignas(16) uint8_t fxsave_buf[512];// Embedded 16-byte aligned FPU buffer
} gth_ctx_t;
```

## Context Switching Flow & ABI Compliance

1. **Yield/block**: Thread calls `gth_thread_yield()` or blocks on a sync primitive.
2. **State save**: `gth_ctx_swap(&current->ctx, &scheduler_ctx)` saves callee-saved registers into the thread's `gth_ctx_t` and restores the scheduler context.
3. **Scheduler picks next**: `gth_scheduler_pick_ready_thread_mode()` selects the next READY thread based on the active mode (fair Round-Robin via `last_rr_slot`, priority, trace replay, or fuzz).
4. **State restore**: `gth_ctx_swap(&scheduler_ctx, &next->ctx)` saves the scheduler state and restores the next thread's registers.
5. **Thread resumes**: The CPU continues executing the new thread from where it was last saved.

First-time thread entry uses `gth_ctx_make()` in assembly (`ctx_x86_64.S`) to set up the initial context. The 64-bit `slot_index` is passed directly in `%rcx` per System V AMD64 ABI standards and stored into `%rax+8` (`fxsave_area`). RIP points to `gth_context_thread_trampoline(size_t slot_index_arg)`, which calls the user's entry function.

## Thread Lifecycle

```
EMPTY -> READY -> RUNNING -> DONE
                   |    ^
                   v    |
                 BLOCKED-+
                   |
                  CANCELED
```

- **EMPTY**: Slot is free.
- **READY**: In the scheduler's ready queue, waiting for a time quantum.
- **RUNNING**: Currently executing on the CPU.
- **BLOCKED**: Waiting on a sync primitive (mutex, semaphore, condition, join).
- **DONE**: Thread function returned. Retval stored. Can be joined once.
- **CANCELED**: Marked for cancellation. Terminates at next scheduling point.

## Scheduler Modes

The scheduler supports four operating modes, selected at runtime:

| Mode | Description |
|------|-------------|
| **NORMAL** | Standard round-robin or priority scheduling. |
| **RECORD** | Normal scheduling, but all events are captured to a trace file. |
| **REPLAY** | Scheduling decisions are driven by a previously recorded trace. |
| **FUZZ** | Scheduling is perturbed by a PRNG to explore different interleavings. |

## Synchronization Primitives & Lost-Wakeup Prevention

All sync objects are opaque, inline-allocated structs (no heap allocation). They use the runtime's internal thread-blocking mechanism with atomic state updates to guarantee lost-wakeup immunity.

| Primitive | Operations | Blocking Semantics & Race Invariants |
|-----------|------------|--------------------------------------|
| **Mutex** | lock, trylock, unlock, destroy | Blocks on lock contention. `gth_wq_is_full()` bounds checks prevent silent queue drops. Main thread (TID 0) non-blocking locks supported. |
| **Semaphore** | init, wait, post, destroy | Counting semaphore. Candidate threads mark state as `GTH_THREAD_BLOCKED` before enqueuing to prevent lost wakeup races. `UINT32_MAX` overflow checks. `gth_sem_destroy()` verifies `!gth_wq_is_empty()`. |
| **Condition Variable** | wait, signal, broadcast, destroy | Atomically transitions thread to `GTH_THREAD_BLOCKED` prior to releasing mutex, ensuring interleaved signals unblock waiters cleanly. |

When a thread blocks on a sync primitive:
1. Its state transitions from `GTH_THREAD_RUNNING` to `GTH_THREAD_BLOCKED` *before* context swapping or queue publication.
2. The scheduler is invoked to pick the next `GTH_THREAD_READY` thread.
3. When the primitive is unblocked/signaled, `gth_thread_unblock_slot()` transitions the thread back to `GTH_THREAD_READY`.

## Trace / Replay / Fuzz System

### Trace Recording & Resource Safety

When `gth_trace_start()` is called, the runtime enters RECORD mode. Every scheduling decision, context switch, and synchronization event is captured to a binary trace file.

- **Double-Close Safety**: Trace cleanup sets `state->trace->file = NULL` immediately following `fclose()` to prevent invalid handle double-free bugs during runtime shutdown.
- **Trace file format (v2)**:
  - 32-byte header: magic ("GTR2"), version, creation time, timestamp frequency.
  - 32-byte event records: timestamp, event type, TID, slot index, event data, FNV-1a checksum.

Buffered writes (256 records per flush) minimize I/O overhead.

### Deterministic Replay

`gth_replay_from()` loads a trace file and enters REPLAY mode. The scheduler selects threads based on the recorded event sequence rather than the normal scheduling policy. This allows exact reproduction of a previously observed execution.

Divergence detection: if the runtime attempts to consume an event that does not match the expected type/TID, the replay is marked as diverged (`GTH_ENOTFOUND`).

### Schedule Fuzzing

Fuzz mode uses an xorshift128+ PRNG to randomly perturb scheduling decisions. The perturbation rate is configurable (0-100%). Seed initialization uses pure SplitMix64 mixing without non-deterministic `rdtsc` timestamp XOR contamination, guaranteeing exact execution reproducibility across runs with identical seeds.

## Memory Management

- **Thread stacks**: Each thread gets a stack allocated via `mmap` with a guard page (`mprotect` PROT_NONE) to detect overflow. System `sysconf(_SC_PAGESIZE)` is cached at startup to optimize allocation speed. Upper stack limit is bounded by `GTH_MAX_STACK_SIZE` (64 MiB).
- **Thread table**: Fixed-size array of 128 slots (`GTH_MAX_THREADS`) with static `_Alignas(16)` structural alignment.
- **Sync objects**: All storage is inline within opaque structs. No heap allocation.
- **Trace buffers**: Heap-allocated during trace recording, safely freed on stop.

## Build Instructions

### Prerequisites

- Linux x86_64
- C compiler with C17 support (GCC or Clang)
- CMake >= 3.20
- CMocka development package (`libcmocka-dev`)
- clang-format (optional, for formatting)

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

### Run Examples

```bash
./build/basic_threads
./build/sync_demo
./build/trace_replay
```

### Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are enabled by default in Debug builds. Disable with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGTHREADS_ENABLE_SANITIZERS=OFF
```

### Format

```bash
cmake --build build --target format
cmake --build build --target format-check
```
