// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <dse/modelc/runtime.h>


/**
Importer for Runtime
====================

This Importer is able to load and execute a Model which implements the
ModelC (i.e. model.h) interface. This importer has additional linked
libraries, the Runtime is expected to provide all necessary objects/symbols.
*/

#define STEP_SIZE MODEL_DEFAULT_STEP_SIZE
#define END_TIME  600
#define STEPS     10


static void __log(const char* format, ...)
{
    printf("Importer: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}


static void __print_sv(SignalVector* sv)
{
    __log("Signal Vectors:");
    while (sv && sv->name) {
        __log("  Name: %s", sv->name);
        __log("    Model Function: %s", sv->function_name);
        __log("    Signal Count  : %d", sv->count);
        if (sv->is_binary) {
            __log("    Vector Type   : binary");
            __log("    Signals       :");
            for (uint32_t i = 0; i < sv->count; i++) {
                __log("      - name       : %s", sv->signal[i]);
                __log("        length     : %d", sv->length[i]);
                __log("        buffer len : %d", sv->buffer_size[i]);
                __log("        mime_type  : %s", sv->mime_type[i]);
                uint8_t* buffer = sv->binary[i];
                for (uint32_t j = 0; j + 16 < sv->length[i]; j += 16) {
                    __log("          %02x %02x %02x %02x %02x %02x %02x %02x "
                          "%02x %02x %02x %02x %02x %02x %02x %02x",
                        buffer[j + 0], buffer[j + 1], buffer[j + 2],
                        buffer[j + 3], buffer[j + 4], buffer[j + 5],
                        buffer[j + 6], buffer[j + 7], buffer[j + 8],
                        buffer[j + 9], buffer[j + 10], buffer[j + 11],
                        buffer[j + 12], buffer[j + 13], buffer[j + 14],
                        buffer[j + 15]);
                }
            }
        } else {
            __log("    Vector Type   : double");
            __log("    Signals       :");
            for (uint32_t i = 0; i < sv->count; i++) {
                __log("      - name : %s", sv->signal[i]);
                __log("        value: %g", sv->scalar[i]);
            }
        }
        /* Next signal vector. */
        sv++;
    }
}


static void __dump_sim(SimulationSpec* sim)
{
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        __log("Model Instance: %s", mi->name);
        __print_sv(mi->model_desc->sv);
    }
}


int main(int argc, char** argv)
{
    if (argc < 4 || argc > 5) {
        __log("Usage: %s <runtime model> <sim path> <sim yaml> [model inst]",
            argv[0]);
        return EINVAL;
    }

    /* Construct the Importer/Runtime descriptor object. */
    RuntimeModelDesc rm = {
        .runtime = {
            .runtime_model = argv[1],
            .sim_path = argv[2],
            .simulation_yaml = argv[3],
            .model_name = argc == 5 ? argv[4] : NULL,
            .end_time = END_TIME,
        },
    };
    static char _cwd[PATH_MAX];
    getcwd(_cwd, PATH_MAX);
    __log("Cwd: %s", _cwd);
    __log("Runtime: %s", rm.runtime.runtime_model);
    __log("Simulation Path: %s", rm.runtime.sim_path);
    __log("Simulation YAML: %s", rm.runtime.simulation_yaml);
    __log("Model: %s", rm.runtime.model_name);

    /* Loading the Model. */
    __log("Loading runtime model: %s ...", rm.runtime.runtime_model);
    void* handle = dlopen(rm.runtime.runtime_model, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        __log("ERROR: dlopen call failed: %s", dlerror());
        __log("Model library not loaded!");
        return ENOSYS;
    }
    ModelVTable vtable = {};
    vtable.create = dlsym(handle, MODEL_CREATE_FUNC_NAME);
    vtable.step = dlsym(handle, MODEL_STEP_FUNC_NAME);
    vtable.destroy = dlsym(handle, MODEL_DESTROY_FUNC_NAME);
    __log("vtable.create: %p", vtable.create);
    __log("vtable.step: %p", vtable.step);
    __log("vtable.destroy: %p", vtable.destroy);
    if (vtable.create == NULL || vtable.step == NULL) {
        __log("vtable not complete!");
        return EINVAL;
    }

    /* Call model_create(). */
    ModelDesc* m = (ModelDesc*)&rm;
    if (vtable.create) {
        __log("Calling vtable.create() ...");
        m = vtable.create(m);
    }
    __dump_sim(rm.model.sim);

    /* Step the model with calls to model_step(). */
    double model_time = 0.0;
    for (int i = 0; i < STEPS; i++) {
        __log("Calling vtable.step(): model_time=%f, stop_time=%f", model_time,
            (model_time + STEP_SIZE));
        int rc = vtable.step(m, &model_time, (model_time + STEP_SIZE));
        if (rc != 0) {
            __log("step() returned error code: %d", rc);
        }
    }
    __dump_sim(rm.model.sim);

    /* Call model_destroy(). */
    if (vtable.destroy) {
        __log("Calling vtable.destroy() ...");
        vtable.destroy(m);
    }
    if ((void*)&rm != (void*)m) free(m);
}
