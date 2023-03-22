// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <assert.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


#define ARRAY_LENGTH(array)      (sizeof((array)) / sizeof((array)[0]))


/* Model Function definitions. */
#define MODEL_FUNCTION_NAME      "example"
#define MODEL_FUNCTION_DO_STEP   do_step
#define MODEL_FUNCTION_STEP_SIZE 0.005
#define MODEL_FUNCTION_CHANNEL   "model_channel"


/* Signals are defined in dynamic_model.yaml, order must match here! */
typedef enum signal_name_index {
    SIGNAL_FOO,
    SIGNAL_BAR,
    __SIGNAL__COUNT__
} signal_name_index;


/* Signal Vector definition (storage allocated during model_setup()). */
static double* signal_value;


/* Model Function do_step() definition. Each Model Function has its own
   do_step() and they are registered during model_setup(). */
int MODEL_FUNCTION_DO_STEP(double* model_time, double stop_time)
{
    assert(signal_value);

    signal_value[SIGNAL_FOO] += 1.2;
    signal_value[SIGNAL_BAR] += 4.2;
    *model_time = stop_time;

    return 0;
}


/* Model Setup: a handle to this function will be dynamically loaded by the
   Controller and it (this function) will be called to initialise the Model
   and its Model Functions. */
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


/* Model Exit: OPTIONAL! a handle to this function, if present, will be
   dynamically loaded by the Controller and called before the Controller exits
   the simulation (and sends the ModelExit message to the SimBus). */
int MODEL_EXIT_FUNC(ModelInstanceSpec* model_instance)
{
    log_notice("Model function " MODEL_EXIT_FUNC_STR " called");
    if (model_instance) {
        /* The parameter model_instance may, in some circumstances, not
           be set. Therefore check before using. */
    }

    return 0;
}
