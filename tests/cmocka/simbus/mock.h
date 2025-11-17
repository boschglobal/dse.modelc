// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TESTS_CMOCKA_SIMBUS_MOCK_H_
#define TESTS_CMOCKA_SIMBUS_MOCK_H_

#include <dse/logger.h>
#include <dse/testing.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>


typedef struct ModelCMock {
    char**      argv;
    size_t      argc;
    const char* model_name;

    ModelCArguments    args;
    SimulationSpec     sim;
    ModelInstanceSpec* mi;
    Endpoint*          endpoint;
    Controller*        controller;
    ModelVTable        vtable;
} ModelCMock;


int mock_setup(ModelCMock* m);
int mock_teardown(ModelCMock* m);


#endif  // TESTS_CMOCKA_SIMBUS_MOCK_H_
