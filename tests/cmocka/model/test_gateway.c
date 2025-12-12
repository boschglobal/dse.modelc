// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dse/logger.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/gateway.h>


#define UNUSED(x) ((void)x)


extern uint8_t __log_level__;

typedef struct ModelCMock {
    Endpoint*   endpoint;
    Controller* controller;
} ModelCMock;


void stub_setup_objects(Controller* c, Endpoint* e);
void stub_release_objects(Controller* c, Endpoint* e);


static int test_setup(void** state)
{
    ModelCMock* mock = calloc(1, sizeof(ModelCMock));
    assert_non_null(mock);

    mock->controller = calloc(1, sizeof(Controller));
    mock->controller->adapter = calloc(1, sizeof(Adapter));
    mock->endpoint = calloc(1, sizeof(Endpoint));
    stub_setup_objects(mock->controller, mock->endpoint);

    /* Return the mock. */
    *state = mock;
    return 0;
}


static int test_teardown(void** state)
{
    ModelCMock* mock = *state;

    stub_release_objects(mock->controller, mock->endpoint);
    if (mock && mock->controller) {
        if (mock->controller->adapter) free(mock->controller->adapter);
        free(mock->controller);
    }
    if (mock && mock->endpoint) free(mock->endpoint);
    if (mock) free(mock);

    return 0;
}


void test_gateway__scalar_sv(void** state)
{
    UNUSED(state);

    double model_time = 0.0;
    double step_size = 0.05;
    double end_time = 0.2;

    ModelGatewayDesc gw;
    SignalVector*    sv;

    /* Setup the gateway. */
    double      gw_foo = 0.0;
    double      gw_bar = 42.0;
    const char* yaml_files[] = {
        "resources/model/gateway.yaml",
        NULL,
    };
    model_gw_setup(
        &gw, "gateway", yaml_files, __log_level__, step_size, end_time);

    /* Find the scalar signal vector. */
    sv = gw.sv;
    while (sv && sv->name) {
        if (strcmp(sv->name, "scalar") == 0) break;
        /* Next signal vector. */
        sv++;
    }
    assert_non_null(sv);
    assert_non_null(sv->name);
    assert_string_equal(sv->name, "scalar");

    /* Run the simulation. */
    ModelInstancePrivate* mip = gw.mi->private;
    AdapterModel*         am = mip->adapter_model;
    while (model_time <= end_time) {
        /* Marshal data _to_ the signal vector. */
        sv->scalar[0] = gw_foo;
        sv->scalar[1] = gw_bar;
        /* Synchronise the gateway. */
        assert_double_equal(model_time, am->model_time, 0.0);
        model_gw_sync(&gw, model_time);
        assert_double_equal(model_time + step_size, am->model_time, 0.0);
        /* Marshal data _from_ the signal vector. */
        gw_foo = sv->scalar[0];
        gw_bar = sv->scalar[1];
        assert_double_equal(gw_foo, sv->scalar[0], 0.0);
        assert_double_equal(gw_bar, sv->scalar[1], 0.0);
        /* Model function. */
        gw_foo += 1;
        gw_bar += gw_foo;
        /* Next step. */
        model_time += step_size;
    }

    /* Exit the simulation. */
    model_gw_exit(&gw);
}


int run_gateway_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_gateway__scalar_sv, s, t),
    };

    return cmocka_run_group_tests_name("GATEWAY", tests, NULL, NULL);
}
