// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <assert.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


/* Model Function definitions. */
#define MODEL_FUNCTION_NAME      "example"
#define MODEL_FUNCTION_DO_STEP   do_step
#define MODEL_FUNCTION_STEP_SIZE 0.005
#define MODEL_FUNCTION_CHANNEL   "model_channel"


/* Signals are defined in signal_group.yaml, order must match here! */
typedef enum signal_name_index {
    SIGNAL_FOO,
    SIGNAL_BAR,
    __SIGNAL__COUNT__
} signal_name_index;

static double* signal_value;


int MODEL_FUNCTION_DO_STEP(double* model_time, double stop_time)
{
    assert(signal_value);

    signal_value[SIGNAL_FOO] += 1.2;
    signal_value[SIGNAL_BAR] += 4.2;
    *model_time = stop_time;

    return 0;
}


int MODEL_SETUP_FUNC(ModelInstanceSpec* model_instance)
{
    log_notice("Model function " MODEL_SETUP_FUNC_STR " called");
    assert(model_instance);

    int rc = model_function_register(model_instance, MODEL_FUNCTION_NAME,
        MODEL_FUNCTION_STEP_SIZE, MODEL_FUNCTION_DO_STEP);
    if (rc != 0) return rc;

    /* Register channels (and get storage). */
    static ModelChannelDesc channel_desc = {
        .name = MODEL_FUNCTION_CHANNEL,
        .function_name = MODEL_FUNCTION_NAME,
    };
    rc = model_configure_channel(model_instance, &channel_desc);
    if (rc != 0) return rc;
    assert(channel_desc.signal_count == __SIGNAL__COUNT__);
    assert(channel_desc.vector_double);
    signal_value = channel_desc.vector_double;

    return 0;
}


int MODEL_EXIT_FUNC(ModelInstanceSpec* model_instance)
{
    log_notice("Model function " MODEL_EXIT_FUNC_STR " called");
    return 0;
}
