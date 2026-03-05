# gthreads

this project is something i have been Procastinating the work on for a long time but now is the time, gthreads as "green-threads" is deterministic user-space threading runtime in C, designed to demonstrate OS/runtime knowleadge into something paractical
notations covered "or may be covered" context switching, stack management, scheduling, synchronization, and reproducible concurrency debugging.

## Project Goal

to build a production-style green-thread library with:

- User-space threads and cooperative/preemptive scheduling (configurable)
- Synchronization primitives (mutex, semaphore, condition variable)
- Linux event-loop integration (`epoll`)
- **Deterministic replay + schedule fuzzing** for race reproduction

## Why this project is differentiated

after researching alot of refrences i found out most green-thread projects stop at Round Robin scheduling. So `gthreads` adds a trace/replay subsystem that records scheduling and synchronization events and replays them deterministically to reproduce nondeterministic bugs.

## Milestone Summary

1. Runtime foundation: context switch, stack guard, create/yield/join
2. Scheduling: RR then priority policy
3. Synchronization primitives + correctness tests
4. Trace/replay/fuzzing subsystem
5. Performance benchmarks and stabilization

## Non-goals (v0.1)

- Kernel-level threading
- Windows/macOS portability (Linux-first)
- Lock-free scheduler internals

## Build & Validation (target)

- Compiler: `gcc` with C17
- Sanitizers: AddressSanitizer + UndefinedBehaviorSanitizer
- Test runner: `ctest` or custom test harness
- Static checks: `clang-tidy` (optional first pass)
