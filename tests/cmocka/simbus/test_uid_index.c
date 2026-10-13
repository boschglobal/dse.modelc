// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <dse/testing.h>
#include <dse/modelc/adapter/uid_index.h>


#define UNUSED(x) ((void)x)


static uint32_t _test_uid(uint32_t index)
{
    return index * 2654435761U;
}


static void test_uid_index_growth_and_lookup(void** state)
{
    UNUSED(state);

    const size_t count = 100000;
    SignalValue* signals = calloc(count, sizeof(*signals));
    assert_non_null(signals);

    SignalValueUidIndex* index = NULL;
    for (uint32_t i = 0; i < count; i++) {
        signals[i].uid = _test_uid(i + 1);
        assert_true(
            signal_value_uid_index_insert(&index, signals[i].uid, &signals[i]));
    }

    for (uint32_t i = 0; i < count; i++) {
        assert_true(
            signal_value_uid_index_find(index, signals[i].uid) == &signals[i]);
    }

    SignalValue duplicate = { .uid = signals[count / 2].uid };
    errno = ENOMEM;
    assert_true(
        signal_value_uid_index_insert(&index, duplicate.uid, &duplicate));
    assert_int_equal(errno, 0);
    assert_true(signal_value_uid_index_find(index, duplicate.uid) ==
                &signals[count / 2]);
    assert_null(signal_value_uid_index_find(index, 0));
    assert_null(signal_value_uid_index_find(index, _test_uid(count + 1)));

    errno = ENOMEM;
    assert_true(signal_value_uid_index_insert(&index, 0, &duplicate));
    assert_int_equal(errno, 0);

    signal_value_uid_index_destroy(index);
    free(signals);
}


int run_uid_index_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_uid_index_growth_and_lookup),
    };

    return cmocka_run_group_tests_name("SIMBUS / UID INDEX", tests, NULL, NULL);
}
