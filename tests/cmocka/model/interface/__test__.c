// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0
// SPDX-License-Identifier: BIOS-4.0

#include <dse/testing.h>


extern int run_model_interface_tests(void);


int main()
{
    int rc = 0;
    rc |= run_model_interface_tests();
    return rc;
}
