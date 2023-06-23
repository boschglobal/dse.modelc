// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <dlfcn.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


#define STEP_SIZE 0.005
#define END_TIME  3600
#define STEPS     10


static void marshal_to_signal_vectors(SignalVector* sv);
static void marshal_from_signal_vectors(SignalVector* sv);
static void print_signal_vectors(SignalVector* sv);


/**
 *  Model Loader and Stepper.
 *
 *  A debug interface for testing Models in isolation from a SimBus.
 *
 *  Example
 *  -------
 *      $ cd dse/modelc/build/_out/examples/dynamic/
 *      $ ../../bin/mstep \
 *          --name=dynamic_model_instance \
 *          stack.yaml \
 *          signal_group.yaml \
 *          model.yaml
 */
int main(int argc, char** argv)
{
    int                rc;
    ModelCArguments    args;
    SimulationSpec     sim;
    ModelInstanceSpec* mi;

    modelc_set_default_args(&args, NULL, STEP_SIZE, END_TIME);
    args.steps = STEPS;
    modelc_parse_arguments(&args, argc, argv, "Model Loader and Stepper");
    if (args.name == NULL) log_fatal("name argument not provided!");


    /* Configure the Model (takes config from parsed YAML docs). */
    rc = modelc_configure(&args, &sim);
    if (rc) exit(rc);
    mi = modelc_get_model_instance(&sim, args.name);
    if (mi == NULL) log_fatal("ModelInstance %s not found!", args.name);


    /* Loading the Model library and symbols. */
    const char* model_filename = mi->model_definition.full_path;
    log_notice("Loading model: %s ...", model_filename);
    void* handle = dlopen(model_filename, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        log_error("ERROR: dlopen call failed: %s", dlerror());
        log_fatal("Model library not loaded!");
    }
    ModelSetupHandler model_setup_func = dlsym(handle, MODEL_SETUP_FUNC_STR);
    ModelExitHandler  model_exit_func = dlsym(handle, MODEL_EXIT_FUNC_STR);
    if (model_setup_func == NULL) log_fatal("model_setup_func not found!");


    /* Call the setup function of the Model. */
    rc = model_setup_func(mi);
    if (rc) log_fatal("Call: model_setup_func failed! (rc=%d)!", rc);
    SignalVector* sv = model_sv_create(mi);
    print_signal_vectors(sv);


    /* Run the Model/Simulation. */
    log_notice("Starting Simulation (for %d steps) ...", args.steps);
    for (uint32_t i = 0; i < args.steps; i++) {
        log_notice("  step %d", i);
        marshal_to_signal_vectors(sv);
        rc = modelc_step(mi, args.step_size);
        if (rc) log_fatal("Call: modelc_step failed! (i=%d, rc=%d)!", i, rc);
        marshal_from_signal_vectors(sv);
    }
    log_notice("Simulation complete.");
    print_signal_vectors(sv);


    /* Call the exit function of the Model. */
    if (model_exit_func) {
        rc = model_exit_func(mi);
        if (rc) log_fatal("Executing model exit failed! (rc=%d)!", rc);
    }

    exit(0);
}


static void marshal_to_signal_vectors(SignalVector* sv)
{
    while (sv && sv->name) {
        /* Next signal vector. */
        sv++;
    }
}


static void marshal_from_signal_vectors(SignalVector* sv)
{
    while (sv && sv->name) {
        /* Next signal vector. */
        sv++;
    }
}


static void print_signal_vectors(SignalVector* sv)
{
    log_notice("Signal Vectors:");
    while (sv && sv->name) {
        log_notice("  Name: %s", sv->name);
        log_notice("    Model Function: %s", sv->function_name);
        log_notice("    Signal Count  : %d", sv->count);
        if (sv->is_binary) {
            log_notice("    Vector Type   : binary");
            log_notice("    Signals       :");
            for (uint32_t i = 0; i < sv->count; i++) {
                log_notice("      - name : %s", sv->signal[i]);
            }
        } else {
            log_notice("    Vector Type   : double");
            log_notice("    Signals       :");
            for (uint32_t i = 0; i < sv->count; i++) {
                log_notice("      - name : %s", sv->signal[i]);
                log_notice("        value: %g", sv->scalar[i]);
            }
        }
        /* Next signal vector. */
        sv++;
    }
}
