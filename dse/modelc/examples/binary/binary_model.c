// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <assert.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


#define ARRAY_LENGTH(array)             (sizeof((array)) / sizeof((array)[0]))


/* Model Function definitions. A single Model Function is defined. */
#define MODEL_FUNCTION_NAME             "binary_example"
#define MODEL_FUNCTION_STEP_SIZE        0.005
#define MODEL_FUNCTION_CHANNEL          "binary_channel"
#define MODEL_FUNCTION_DO_STEP_FUNC     do_step
#define MODEL_FUNCTION_DO_STEP_FUNC_STR "do_step"


/* Signals are defined in dynamic_model.yaml, order must match here! */
typedef enum signal_name_index {
    SIGNAL_RAW,
    __SIGNAL__COUNT__
} signal_name_index;


/* Signal Vector (binary). */
static void**    signal_value;
static uint32_t* signal_value_size;
static uint32_t* signal_value_buffer_size;


/* Model Function do_step() definition. Each Model Function has its own
   do_step() and they are registered during model_setup(). */
int MODEL_FUNCTION_DO_STEP_FUNC(double* model_time, double stop_time)
{
    log_simbus("Model function " MODEL_FUNCTION_DO_STEP_FUNC_STR " called");

    /* Define some test data. */
    static int         index = 0;
    static const char* test_data[4] = {
        "one",
        "two",
        "three",
        "four",
    };

    /* Print the incoming content of the binary signal. */
    if (signal_value_size[SIGNAL_RAW]) {
        /* The binary string may have embedded NULL characters, replace
        them before printing the buffer. */
        char* buffer = (char*)signal_value[SIGNAL_RAW];
        for (uint32_t i = 0; i < signal_value_size[SIGNAL_RAW] - 1; i++) {
            if (buffer[i] == 0) {
                buffer[i] = ' ';
            }
        }
        /* Now print the buffer*/
        log_notice("RECV: %s (buffer size=%u)", signal_value[SIGNAL_RAW],
            signal_value_buffer_size[SIGNAL_RAW]);
    }

    /* Indicate the binary object/signal was consumed (important!). */
    signal_value_size[SIGNAL_RAW] = 0;

    /* Write some test data to the binary signal. */
    if (index < 4) {
        dse_buffer_append(&signal_value[SIGNAL_RAW],
            &signal_value_size[SIGNAL_RAW],
            &signal_value_buffer_size[SIGNAL_RAW], test_data[index],
            strlen(test_data[index]) + 1);
        index += 1;
    }

    /* Advance the model time. */
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
        MODEL_FUNCTION_STEP_SIZE, MODEL_FUNCTION_DO_STEP_FUNC);
    if (rc != 0) return rc;

    /* Register channels (and get storage). */
    static ModelChannelDesc channel_desc = {
        .name = MODEL_FUNCTION_CHANNEL,
        .function_name = MODEL_FUNCTION_NAME,
    };
    rc = model_configure_channel(model_instance, &channel_desc);
    if (rc != 0) return rc;
    if (channel_desc.vector_binary == NULL) {
        log_fatal("Binary vector not allocated!");
    }
    assert(channel_desc.signal_count == __SIGNAL__COUNT__);
    assert(channel_desc.vector_binary);
    assert(channel_desc.vector_binary_size);
    assert(channel_desc.vector_binary_buffer_size);
    signal_value = channel_desc.vector_binary;
    signal_value_size = channel_desc.vector_binary_size;
    signal_value_buffer_size = channel_desc.vector_binary_buffer_size;

    return 0;
}
