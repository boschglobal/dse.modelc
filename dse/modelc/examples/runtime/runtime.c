// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/runtime.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>


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
    char* csv_file =
        dse_path_cat(m->runtime.sim_path, "model/input/data/input.csv");

#if defined(__linux__)
    if (setenv("CSV_FILE", csv_file, 1) != 0) {
        fprintf(stderr, "Unable to set CSV_FILE environment variable\n");
        fflush(stderr);
    }

#define MSG "Hello from importer runtime!"
    printf("Importer: Runtime: set_env(MSG=%s)\n", MSG);
    fflush(stdout);
    setenv("MSG", MSG, true);
#endif

    free(csv_file);
}

static int runtime_virtual_step(
    ModelDesc* model, double* model_time, double stop_time)
{
    ModelSignalIndex message = signal_index(model, "pdu", "message");
    if (message.binary == NULL) return -EINVAL;

    NCODEC* ncodec = signal_codec(message.sv, message.signal);
    if (ncodec == NULL) return -EINVAL;

    NCodecPdu pdu = {};
    int       len;
    while ((len = ncodec_read(ncodec, &pdu)) >= 0) {
        printf("Importer: PDU id=%u ecu_id=%u swc_id=%u %.*s\n", pdu.id,
            pdu.ecu_id, pdu.swc_id, (int)pdu.payload_len, (char*)pdu.payload);
    }
    ncodec_truncate(ncodec);
    *model_time = stop_time;
    return 0;
}


ModelDesc* model_create(ModelDesc* model)
{
    RuntimeModelDesc* m = (RuntimeModelDesc*)model;

    m->model.sim = calloc(1, sizeof(SimulationSpec));
    m->runtime.vtable.set_env = runtime_set_env;
    m = model_runtime_create(m);

    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(m->model.sim, m->runtime.virtual_model_name);
    if (virtual_model == NULL || virtual_model->model_desc == NULL) {
        fprintf(stderr, "Runtime Virtual Model not available\n");
        exit(EXIT_FAILURE);
    }
    m->model.index = virtual_model->model_desc->index;
    m->model.mi = virtual_model;
    m->model.sv = virtual_model->model_desc->sv;
    virtual_model->model_desc->vtable.step = runtime_virtual_step;
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
