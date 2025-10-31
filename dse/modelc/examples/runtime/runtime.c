// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x) ((void)x)


/**
Runtime for Importer Interface
==============================

Implements the importer interface, in this case ModelC (i.e. model.h),
and then runs the simulation via calls to the Model Runtime API.

[importer] -> [importer interface] -> [model_runtime] -> [model]
*/

static void runtime_set_env(RuntimeModelDesc* m)
{
    UNUSED(m);
#if defined(__linux__)
#define MSG "Hello from importer runtime!"
    printf("Importer: Runtime: set_env(MSG=%s)\n", MSG);
    fflush(stdout);
    setenv("MSG", MSG, true);
#endif
}

ModelDesc* model_create(ModelDesc* model)
{
    RuntimeModelDesc* m = (RuntimeModelDesc*)model;

    m->model.sim = calloc(1, sizeof(SimulationSpec));
    m->runtime.vtable.set_env = runtime_set_env;
    m = model_runtime_create(m);
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    RuntimeModelDesc* m = (RuntimeModelDesc*)model;

    int rc = model_runtime_step(m, model_time, stop_time);
    return rc;
}


void model_destroy(ModelDesc* model)
{
    RuntimeModelDesc* m = (RuntimeModelDesc*)model;

    model_runtime_destroy(m);
    if (m->model.sim) free(m->model.sim);
    m->model.sim = NULL;
}
