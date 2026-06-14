# gthreads TDD Matrix

Date: 2026-03-02

## Testing Principles

1. Write failing tests before runtime implementation for every non-trivial behavior.
2. Encode discovered concurrency bugs as deterministic replay regressions.
3. Keep tests deterministic by default (fixed seeds and explicit timeouts).
4. Regression tests cover lifecycle, scheduler fairness, stack safety, and state cleanup after shutdown.

## Test Matrix

| Area | Test ID | Type | Scenario | Expected Result |
|---|---|---|---|---|
| Foundation | TDD-001 | Unit | Create thread with null function pointer | `GTH_EINVAL` returned |
| Foundation | TDD-002 | Unit | Create thread then yield | New thread runs and exits cleanly |
| Foundation | TDD-003 | Integration | Join completed thread | Join returns `GTH_OK` and captures retval |
| Foundation | TDD-004 | Safety | Stack guard page overflow trigger | Runtime detects and aborts thread safely |
| Scheduler | TDD-005 | Integration | RR fairness with equal workload | Bounded scheduling skew across threads |
| Scheduler | TDD-006 | Integration | Priority scheduling preference | Higher-priority thread runs before lower-priority thread |
| Scheduler | TDD-007 | Stress | 1,000 short-lived threads | No memory leaks or invalid state transitions |
| Regression | TDD-017 | Regression | Runtime init rejects undersized stacks | `GTH_EINVAL` returned and runtime remains uninitialized |
| Regression | TDD-018 | Regression | Join on unknown tid fails cleanly | `GTH_ENOTFOUND` returned without mutating runtime stats |
| Regression | TDD-019 | Regression | Cancel on unknown tid fails cleanly | `GTH_ENOTFOUND` returned without changing active threads |
| Regression | TDD-020 | Regression | Runtime shutdown resets all counters | Stats return zeroed counters after shutdown/reinit |
| Regression | TDD-021 | Regression | Context test suite is deterministic | Guard-page contract tests do not depend on timing |
| Sync | TDD-008 | Integration | Mutex lock/unlock with contention | Mutual exclusion preserved |
| Sync | TDD-009 | Integration | Semaphore producer/consumer | No dropped or duplicated items |
| Sync | TDD-010 | Integration | Condition wait/signal ordering | Waiters wake according to signaling behavior |
| Replay/Fuzz | TDD-011 | Integration | Trace capture for scheduler events | Trace file contains complete ordered event sequence |
| Replay/Fuzz | TDD-012 | Regression | Replay previously captured trace | Reproduces identical event and outcome sequence |
| Replay/Fuzz | TDD-013 | Determinism | Same fuzz seed across runs | Identical scheduler perturbation pattern |
| Replay/Fuzz | TDD-014 | Negative | Replay with tampered trace | Runtime reports divergence with explicit error |
| Stability | TDD-015 | Performance | Context-switch benchmark | Stable baseline metric generated |
| Stability | TDD-016 | Stress | Mixed sync + replay workload | No deadlock, no crash, deterministic replay passes |

## Test File Draft Map

- `tests/unit/test_api_validation.c`
- `tests/unit/test_queue.c`
- `tests/integration/test_lifecycle.c`
- `tests/integration/test_scheduler_rr.c`
- `tests/integration/test_scheduler_priority.c`
- `tests/integration/test_sync_mutex.c`
- `tests/integration/test_sync_sem.c`
- `tests/integration/test_sync_cond.c`
- `tests/replay/test_trace_capture.c`
- `tests/replay/test_trace_replay.c`
- `tests/replay/test_fuzz_seed_determinism.c`
- `tests/replay/test_replay_regressions.c`
- `tests/stress/test_mass_threads.c`

## Deterministic Replay Regression Policy

Any concurrency bug found in stress/fuzz testing must be converted into:

1. A trace artifact (`tests/replay/traces/<bug-id>.gthtrace`)
2. A replay regression test (`tests/replay/test_bug_<bug-id>.c`)
3. A short root-cause note in `docs/testing/replay-regressions.md`

Regression tests should be deterministic and should assert:

- Runtime state is reset after shutdown
- Invalid input paths return stable error codes
- Scheduler behavior remains bounded for equal-priority workloads
- Stack-related validation fails before any thread starts

## Validation Commands (draft)

Exact commands will be finalized once build files are added. Target checks:

- Debug build with sanitizers
- Full test suite run
- Replay-regression subset run
