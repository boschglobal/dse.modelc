// Copyright 2023 Robert Bosch GmbH

#include <dse/testing.h>

#define UNUSED(x) ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


typedef struct FooMock {
    int foo_value;
} FooMock;

static int test_setup(void** state)
{
    FooMock* mock = calloc(1, sizeof(FooMock));
    mock->foo_value = 4;
    *state = mock;
    return 0;
}

static int test_teardown(void** state)
{
    FooMock* mock = *state;
    if (mock) free(mock);
    return 0;
}

typedef struct FooTest {
    int test_value;
    int remainder;
}

void test_foo__data_driven(void** state)
{
    FooMock* mock = *state;
    FooTest tests[] =  {
        { .test_value = 5, .remainder = 1 },
        { .test_value = 8, .remainder = 4 },
        { .test_value = 2, .remainder = -2 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(tests); i++) {
        int remainder = tests[i].test_value - mock->foo_value;
        assert_int_equal(remainder, tests[i].remainder);
    }
}

int run_foo_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_foo__data_driven, s, t),
    };
    return cmocka_run_group_tests_name("FOO", tests, NULL, NULL);
}
