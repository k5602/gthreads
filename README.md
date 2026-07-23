# gthreads

Deterministic user-space threading runtime in C17 for Linux x86_64.

Cooperative green threads, hand-written x86_64 context switching, deterministic trace/replay, and schedule fuzzing.

## Why this exists

This project was my way back into Systems especially OS internals after my time in vulnera - building something real from scratch where every line has to make sense. Context switches, stack management, scheduler design, sync primitives and assembly - the stuff you don't learn by prompting like the nonsense happening now.

I'll come back to it now and then, but the point was never to ship a library. The point was to understand what's actually happening under the hood so this an educational project.

Thanks to prof. Andrea Arpaci-Dusseau and prof. Remzi Arpaci-Dusseau for their great work on operating systems Course and Book.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Requires: Linux x86_64, CMake >= 3.20, GCC/Clang (C17), `libcmocka-dev`.

## Performance

Measured baseline latency under ASan/UBSan instrumentation:

| Operation | Latency / Throughput |
|-----------|----------------------|
| Context switch (yield) | ~330 ns |
| Thread create + join | ~6,000 ns |
| Mutex lock + unlock | ~160 ns |
| Semaphore wait + post | ~210 ns (4.6M ops/sec) |

## Quick start

```c
#include <gthreads/gthreads.h>

static void *worker(void *arg) {
    printf("thread %lu\n", (unsigned long)gth_thread_self());
    return NULL;
}

int main(void) {
    gth_runtime_init(&(gth_runtime_config_t){ .stack_size_bytes = 65536 });
    gth_tid_t tid;
    gth_thread_create(&tid, NULL, worker, NULL);
    gth_thread_join(tid, NULL);
    gth_runtime_shutdown();
}
```

## API

| Category | Functions |
|----------|-----------|
| Runtime | `gth_runtime_init`, `gth_runtime_shutdown`, `gth_runtime_get_stats` |
| Threads | `gth_thread_create`, `gth_thread_join`, `gth_thread_yield`, `gth_thread_cancel`, `gth_thread_self` |
| Mutex | `gth_mutex_init`, `gth_mutex_lock`, `gth_mutex_trylock`, `gth_mutex_unlock`, `gth_mutex_destroy` |
| Semaphore | `gth_sem_init`, `gth_sem_wait`, `gth_sem_post`, `gth_sem_destroy` |
| Condition | `gth_cond_init`, `gth_cond_wait`, `gth_cond_signal`, `gth_cond_broadcast`, `gth_cond_destroy` |
| Trace/Replay | `gth_trace_start`, `gth_trace_stop`, `gth_replay_from` |
| Fuzz | `gth_fuzz_get_stats`, `gth_fuzz_set_rate` |

All functions return `gth_status_t` (0 = success). Full docs in `include/gthreads/gthreads.h`.

## Examples

See `examples/` - basic_threads, sync_demo, trace_replay.

## Docs

- [Architecture](docs/architecture/ARCHITECTURE.md) - module map, data structures, context switching flow
- [TDD Matrix](docs/testing/TDD-MATRIX.md) - test plan by feature area
