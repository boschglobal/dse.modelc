// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dse/logger.h>


extern uint8_t __log_level__; /* LOG_ERROR LOG_INFO LOG_DEBUG LOG_TRACE */


int  test_gateway_setup(void** state);
int  test_gateway_teardown(void** state);
void test_gateway__scalar_sv(void** state);

void test_model__(void** state);

void test_schema__(void** state);

int  test_signal_setup(void** state);
int  test_signal_teardown(void** state);
void test_signal__scalar(void** state);
void test_signal__binary(void** state);
void test_signal__annotations(void** state);


int main()
{
    __log_level__ = LOG_QUIET;

    const struct CMUnitTest tests[] = {
        /* test_gateway.c */
        cmocka_unit_test_setup_teardown(
            test_gateway__scalar_sv, test_gateway_setup, test_gateway_teardown),

        /* test_model.c */
        // cmocka_unit_test(test_model__),

        /* test_schema.c */
        // cmocka_unit_test(test_schema__),

        /* test_signal.c */
        cmocka_unit_test_setup_teardown(
            test_signal__scalar, test_signal_setup, test_signal_teardown),
        cmocka_unit_test_setup_teardown(
            test_signal__binary, test_signal_setup, test_signal_teardown),
        cmocka_unit_test_setup_teardown(
            test_signal__annotations, test_signal_setup, test_signal_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
