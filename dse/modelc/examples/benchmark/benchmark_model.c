// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>

#define ARRAY_LENGTH(array)      (sizeof((array)) / sizeof((array)[0]))

/* Model Function definitions. */
#define MODEL_FUNCTION_NAME      "example"
#define MODEL_FUNCTION_DO_STEP   do_step
#define MODEL_FUNCTION_STEP_SIZE 0.005
#define MODEL_FUNCTION_CHANNEL   "model_channel"
#define SIGNAL_CHANGE            atoi(getenv("SIGNAL_CHANGE"))


/* Signal Vector storage. */
static double* signal_value;
static const char**  signal_names;
static size_t  signal_count;


int generate_random_num(int lower, int upper)
{
    static unsigned int seed = time(0);
    lower = upper/2 - 1;
    int num = (rand_r(&seed) % (upper  - lower )) + lower;
    return num;
}


int MODEL_FUNCTION_DO_STEP(double* model_time, double stop_time)
{
    size_t count;
    if (SIGNAL_CHANGE == 0) {
        /* All signals from signal_group.yaml are used. */
        count = signal_count;
    } else {
        /* Generate a random count of signals to use. */
        count = generate_random_num(1, SIGNAL_CHANGE);
        if (count > signal_count) count = signal_count;
    }
    log_info("Signal count is : %d", count);

    for (size_t i = 0; i < count; i++) {
        signal_value[i] += 1.2;
    }
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
    assert(channel_desc.vector_double);
    signal_value = channel_desc.vector_double;
    signal_names = channel_desc.signal_names;
    signal_count = channel_desc.signal_count;
    return 0;
}


int MODEL_EXIT_FUNC(ModelInstanceSpec* model_instance)
{
    assert(model_instance);
    log_notice("Model function " MODEL_EXIT_FUNC_STR " called");
    log_notice("Total Signal count from signal yaml file : %d", signal_count);
    log_notice(
        "Signal count in each STEP is in between 1 and %d", SIGNAL_CHANGE);
    return 0;
}
