#include <setjmp.h>
#include <stddef.h>

#include <cmocka.h>

void test_api_validation(void **state);
void test_runtime_executes_and_joins_threads(void **state);
void test_runtime_join_drives_ready_thread(void **state);
void test_runtime_rejects_too_small_stack_config(void **state);
void test_runtime_accepts_guarded_stack_config(void **state);
void test_thread_creation_requires_initialized_runtime(void **state);
void test_thread_creation_rejects_null_function(void **state);
void test_join_on_unstarted_thread_fails_cleanly(void **state);
void test_stack_guard_overflow_is_detected(void **state);
void test_join_rejects_null_tid(void **state);
void test_cancel_rejects_null_tid(void **state);
void test_runtime_shutdown_resets_state(void **state);
void test_replay_and_trace_interfaces_validate_paths(void **state);
void test_priority_scheduler_prefers_higher_priority_thread(void **state);
void test_round_robin_threads_complete_under_load(void **state);
void test_mass_thread_creation_limit_is_reported(void **state);
void test_m2_rr_fairness_regression(void **state);
void test_m2_lifecycle_stress_regression(void **state);
void test_context_yield_resumes_correctly(void **state);
void test_m2_mass_1000_threads_stress(void **state);
void test_mutex_mutual_exclusion_with_contention(void **state);
void test_mutex_trylock_returns_busy_when_held(void **state);
void test_mutex_blocking_on_contention(void **state);
void test_mutex_init_destroy_cycle(void **state);
void test_mutex_rejects_null(void **state);
void test_sem_producer_consumer(void **state);
void test_sem_fifo_wake_order(void **state);
void test_sem_count_tracks_correctly(void **state);
void test_sem_rejects_null(void **state);
void test_cond_wait_signal_single(void **state);
void test_cond_broadcast_wakes_all(void **state);
void test_cond_signal_no_waiters_is_noop(void **state);
void test_cond_rejects_null(void **state);

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_api_validation),
        cmocka_unit_test(test_runtime_executes_and_joins_threads),
        cmocka_unit_test(test_runtime_join_drives_ready_thread),
        cmocka_unit_test(test_runtime_rejects_too_small_stack_config),
        cmocka_unit_test(test_runtime_accepts_guarded_stack_config),
        cmocka_unit_test(test_thread_creation_requires_initialized_runtime),
        cmocka_unit_test(test_thread_creation_rejects_null_function),
        cmocka_unit_test(test_join_on_unstarted_thread_fails_cleanly),
        cmocka_unit_test(test_stack_guard_overflow_is_detected),
        cmocka_unit_test(test_join_rejects_null_tid),
        cmocka_unit_test(test_cancel_rejects_null_tid),
        cmocka_unit_test(test_runtime_shutdown_resets_state),
        cmocka_unit_test(test_replay_and_trace_interfaces_validate_paths),
        cmocka_unit_test(test_priority_scheduler_prefers_higher_priority_thread),
        cmocka_unit_test(test_round_robin_threads_complete_under_load),
        cmocka_unit_test(test_mass_thread_creation_limit_is_reported),
        cmocka_unit_test(test_m2_rr_fairness_regression),
        cmocka_unit_test(test_m2_lifecycle_stress_regression),
        cmocka_unit_test(test_context_yield_resumes_correctly),
        cmocka_unit_test(test_m2_mass_1000_threads_stress),
        cmocka_unit_test(test_mutex_mutual_exclusion_with_contention),
        cmocka_unit_test(test_mutex_trylock_returns_busy_when_held),
        cmocka_unit_test(test_mutex_blocking_on_contention),
        cmocka_unit_test(test_mutex_init_destroy_cycle),
        cmocka_unit_test(test_mutex_rejects_null),
        cmocka_unit_test(test_sem_producer_consumer),
        cmocka_unit_test(test_sem_fifo_wake_order),
        cmocka_unit_test(test_sem_count_tracks_correctly),
        cmocka_unit_test(test_sem_rejects_null),
        cmocka_unit_test(test_cond_wait_signal_single),
        cmocka_unit_test(test_cond_broadcast_wakes_all),
        cmocka_unit_test(test_cond_signal_no_waiters_is_noop),
        cmocka_unit_test(test_cond_rejects_null),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
