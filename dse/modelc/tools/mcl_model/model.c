// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/model.h>
#include <dse/modelc/mcl.h>
#include <dse/logger.h>


#define ARRAY_LENGTH(array)      (sizeof((array)) / sizeof((array)[0]))


/* Model Function definitions. */
#define MODEL_FUNCTION_NAME      "mcl_model"
#define MODEL_FUNCTION_DO_STEP   do_step
#define MODEL_FUNCTION_STEP_SIZE 0.005
#define MODEL_FUNCTION_CHANNEL   "mcl_channel"


/* Copy of MCL Instance for DO_STEP(). */
static MclInstanceDesc* __mcl_instance;

static int __mcl_execute(MclStrategyAction action)
{
    assert(__mcl_instance);
    assert(__mcl_instance->strategy);
    assert(__mcl_instance->strategy->execute_func);

    int rc = __mcl_instance->strategy->execute_func(__mcl_instance, action);
    return rc;
}


/* Model Function do_step() definition. */
int MODEL_FUNCTION_DO_STEP(double* model_time, double stop_time)
{
    log_debug("Model function do_step() called: model_time=%f, stop_time=%f",
        *model_time, stop_time);
    MclInstanceDesc* mcl_instance = __mcl_instance;
    assert(mcl_instance);
    assert(mcl_instance->channel);
    assert(mcl_instance->strategy);
    assert(mcl_instance->strategy->mcl_instance);

    /* Marshall data from Channel to MCL Models. */
    mcl_instance->strategy->execute(MCL_STRATEGY_ACTION_MARSHALL_OUT);
    /* Execute the step strategy (i.e. call step_func() on all MCL Models). */
    log_debug("Call MCL Strategy: step_func ...");
    mcl_instance->strategy->model_time = *model_time;
    mcl_instance->strategy->stop_time = stop_time;
    int rc = mcl_instance->strategy->execute(MCL_STRATEGY_ACTION_STEP);
    if (rc != 0) return rc;
    /* Marshall data from Channel to MCL Models. */
    mcl_instance->strategy->execute(MCL_STRATEGY_ACTION_MARSHALL_IN);
    /* Set the model_time to the *finalised* stop_time of the strategy. */
    *model_time = mcl_instance->strategy->stop_time;

    return rc;
}


/* Model Setup: a handle to this function will be dynamically loaded by the
   Controller and it (this function) will be called to initialise the Model
   and its Model Functions. */
int MODEL_SETUP_FUNC(ModelInstanceSpec* model_instance)
{
    log_notice("Model function " MODEL_SETUP_FUNC_STR " called");
    assert(model_instance);
    int rc = 0;

    /* Register this Model Function with the SimBus/ModelC. */
    rc = model_function_register(model_instance, MODEL_FUNCTION_NAME,
        MODEL_FUNCTION_STEP_SIZE, MODEL_FUNCTION_DO_STEP);
    if (rc != 0) return rc;

    /* Register channels (and get storage). */
    static ModelChannelDesc channel_desc = {
        .name = MODEL_FUNCTION_CHANNEL,
        .function_name = MODEL_FUNCTION_NAME,
    };
    rc = model_configure_channel(model_instance, &channel_desc);
    if (rc != 0) return rc;
    log_debug("%p", channel_desc.vector_double);
    assert(channel_desc.vector_double);

    /* Load the MCL dll's. */
    log_debug("Load the MCL(s) ...");
    errno = 0;
    rc = mcl_load(model_instance);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_error("Failed to load the configured MCL(s)!");
        return rc;
    }

    /* Create the MCL Instance. */
    log_debug("Create the MCL Instance ...");
    errno = 0;
    rc = mcl_create(model_instance);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_error("Failed to create the MCL Instance!");
        return rc;
    }

    /* Save a reference to MCL Instance for the do_step(). */
    assert(model_instance->private);
    ModelInstancePrivate* mip = model_instance->private;
    assert(mip->mcl_instance);
    __mcl_instance = mip->mcl_instance; /* Save for use by do_step(). */
    MclInstanceDesc* mcl_instance = __mcl_instance;
    /* Set the MCL Instance Channel to the Channel created here. */
    mcl_instance->channel = &channel_desc;
    /* Set the MCL Strategy execute function. */
    mcl_instance->strategy->execute = __mcl_execute;

    /* Execute the load strategy (i.e. call load_func() on all MCL Models). */
    log_debug("Call MCL Strategy: load_func ...");
    rc = mcl_instance->strategy->execute(MCL_STRATEGY_ACTION_LOAD);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_error("MCL strategy ACTION_LOAD failed!");
        return rc;
    }

    /* Execute the Init strategy (i.e. call load_func() on all MCL Models). */
    log_debug("Call MCL Strategy: init_func ...");
    rc = mcl_instance->strategy->execute(MCL_STRATEGY_ACTION_INIT);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_error("MCL strategy ACTION_INIT failed!");
        return rc;
    }

    return 0;
}


/* Model Exit: called before the Controller exits the simulation (and sends
   the SyncExit message to the SimBus). */
int MODEL_EXIT_FUNC(ModelInstanceSpec* model_instance)
{
    log_notice("Model function " MODEL_EXIT_FUNC_STR " called");
    assert(model_instance);
    assert(model_instance->private);
    ModelInstancePrivate* mip = model_instance->private;
    assert(mip->mcl_instance);
    MclInstanceDesc* mcl_instance = mip->mcl_instance;
    assert(mcl_instance->strategy);
    assert(mcl_instance->strategy->mcl_instance);

    /* Execute the unload strategy (i.e. call unload_func() on all MCL Models).
     */
    log_debug("Call MCL Strategy: unload_func ...");
    int rc = mcl_instance->strategy->execute(MCL_STRATEGY_ACTION_UNLOAD);
    mcl_destroy(model_instance);
    /* Set the local copy of mcl_instance to NULL. */
    __mcl_instance = NULL;

    return rc;
}
