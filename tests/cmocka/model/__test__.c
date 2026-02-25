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


extern int run_gateway_tests(void);
extern int run_signal_tests(void);
extern int run_transform_tests(void);
extern int run_ncodec_can_tests(void);
extern int run_ncodec_pdu_tests(void);
extern int run_stack_tests(void);
extern int run_model_pdu_tests(void);


int main()
{
    __log_level__ = LOG_QUIET;

    int rc = 0;
    rc |= run_gateway_tests();
    rc |= run_signal_tests();
    rc |= run_transform_tests();
    rc |= run_ncodec_can_tests();
    rc |= run_ncodec_pdu_tests();
    rc |= run_stack_tests();
    rc |= run_model_pdu_tests();
    return rc;
}
