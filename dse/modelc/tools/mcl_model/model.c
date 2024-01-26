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


#define MCL_CHANNEL   "mcl_channel"


typedef struct {
    ModelDesc model;
    /* MCL Properties. */
    MclInstanceDesc* mcl_instance;
} MclExtendedModelDesc;


static int __mcl_execute(ModelDesc* model, MclStrategyAction action)
{
    MclExtendedModelDesc* m = (MclExtendedModelDesc*)model;
    MclInstanceDesc* mcl_instance = m->mcl_instance;
    assert(mcl_instance->strategy);
    assert(mcl_instance->strategy->execute_func);

    int rc = mcl_instance->strategy->execute_func(mcl_instance, action);
    return rc;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    MclExtendedModelDesc* m = calloc(1, sizeof(MclExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));
    ModelInstanceSpec* mi = m->model.mi;

     /* Load the MCL dll's. */
    log_debug("Load the MCL(s) ...");
    errno = 0;
    int rc = mcl_load(mi);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_fatal("Failed to load the configured MCL(s)!");
    }

    /* Create the MCL Instance. */
    log_debug("Create the MCL Instance ...");
    errno = 0;
    rc = mcl_create(mi);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_fatal("Failed to create the MCL Instance!");
    }

    /* Save a reference to MCL Instance for the do_step(). */
    assert(mi->private);
    ModelInstancePrivate* mip = mi->private;
    assert(mip->mcl_instance);
    MclInstanceDesc* mcl_instance = m->mcl_instance = mip->mcl_instance;

    /* Set the MCL Channel (should be the first/only SV). */
    assert(strcmp(m->model.sv->name, MCL_CHANNEL) == 0);
    mcl_instance->mcl_channel_sv = m->model.sv;
    /* Set the MCL Strategy execute function. */
    mcl_instance->strategy->execute = __mcl_execute;

    /* Execute the load strategy (i.e. call load_func() on all MCL Models). */
    log_debug("Call MCL Strategy: load_func ...");
    rc = mcl_instance->strategy->execute(model, MCL_STRATEGY_ACTION_LOAD);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_fatal("MCL strategy ACTION_LOAD failed!");
    }

    /* Execute the Init strategy (i.e. call load_func() on all MCL Models). */
    log_debug("Call MCL Strategy: init_func ...");
    rc = mcl_instance->strategy->execute(model, MCL_STRATEGY_ACTION_INIT);
    if (rc) {
        if (errno == 0) errno = ECANCELED;
        log_fatal("MCL strategy ACTION_INIT failed!");
    }

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    MclExtendedModelDesc* m = (MclExtendedModelDesc*)model;
    MclInstanceDesc* mcl_instance = m->mcl_instance;
    assert(mcl_instance);
    //assert(mcl_instance->channel);
    assert(mcl_instance->strategy);
    assert(mcl_instance->strategy->mcl_instance);

    /* Marshall data from Channel to MCL Models. */
    mcl_instance->strategy->execute(model, MCL_STRATEGY_ACTION_MARSHALL_OUT);
    /* Execute the step strategy (i.e. call step_func() on all MCL Models). */
    log_debug("Call MCL Strategy: step_func ...");
    mcl_instance->strategy->model_time = *model_time;
    mcl_instance->strategy->stop_time = stop_time;
    int rc = mcl_instance->strategy->execute(model, MCL_STRATEGY_ACTION_STEP);
    if (rc != 0) return rc;
    /* Marshall data from Channel to MCL Models. */
    mcl_instance->strategy->execute(model, MCL_STRATEGY_ACTION_MARSHALL_IN);
    /* Set the model_time to the *finalised* stop_time of the strategy. */
    *model_time = mcl_instance->strategy->stop_time;

    return rc;
}


void model_destroy(ModelDesc* model)
{
    MclExtendedModelDesc* m = (MclExtendedModelDesc*)model;
    ModelInstanceSpec* mi = m->model.mi;
    MclInstanceDesc* mcl_instance = m->mcl_instance;
    assert(mcl_instance->strategy);
    assert(mcl_instance->strategy->mcl_instance);

    log_notice("Model function " MODEL_DESTROY_FUNC_NAME " called");

    /* Execute the unload strategy. */
    log_debug("Call MCL Strategy: unload_func ...");
    mcl_instance->strategy->execute(model, MCL_STRATEGY_ACTION_UNLOAD);
    mcl_destroy(mi);
}
