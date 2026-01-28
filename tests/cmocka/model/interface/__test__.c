// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>


extern int run_model_examples_tests(void);
extern int run_model_api_tests(void);
extern int run_pdunet_ncodec_tests(void);


int main()
{
    int rc = 0;
    rc |= run_model_api_tests();
    rc |= run_model_examples_tests();
    rc |= run_pdunet_ncodec_tests();
    return rc;
}
