// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <dse/testing.h>
#include <dse/logger.h>


extern uint8_t __log_level__;
extern int     run_runtime_tests(void);


int main(void)
{
    __log_level__ = LOG_QUIET;
    return run_runtime_tests();
}
