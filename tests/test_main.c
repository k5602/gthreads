#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

void test_api_validation(void **state);
void test_runtime_executes_and_joins_threads(void **state);
void test_runtime_join_drives_ready_thread(void **state);

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_api_validation),
        cmocka_unit_test(test_runtime_executes_and_joins_threads),
        cmocka_unit_test(test_runtime_join_drives_ready_thread),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
