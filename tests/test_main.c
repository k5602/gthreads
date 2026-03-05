#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

void test_api_validation(void **state);
void test_runtime_skeleton(void **state);

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_api_validation),
        cmocka_unit_test(test_runtime_skeleton),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
