// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>


extern uint8_t __log_level__; /* LOG_ERROR LOG_INFO LOG_DEBUG LOG_TRACE */


extern int run_uid_index_tests(void);


int main()
{
    __log_level__ = LOG_QUIET;

    int rc = 0;
    rc |= run_uid_index_tests();
    return rc;
}
