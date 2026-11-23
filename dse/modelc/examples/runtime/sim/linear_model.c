// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model.h>
#include <errno.h>
#include <stdlib.h>


typedef struct LinearModel {
    ModelDesc model;
    double*   input;
    double*   factor;
    double*   offset;
    double*   output;
} LinearModel;


ModelDesc* model_create(ModelDesc* model)
{
    LinearModel* linear = calloc(1, sizeof(LinearModel));
    linear->model = *model;
    linear->input = signal_index(model, "linear_vector", "input").scalar;
    linear->factor = signal_index(model, "linear_vector", "factor").scalar;
    linear->offset = signal_index(model, "linear_vector", "offset").scalar;
    linear->output = signal_index(model, "linear_vector", "output").scalar;
    if (!linear->input || !linear->factor || !linear->offset ||
        !linear->output) {
        free(linear);
        return NULL;
    }
    return (ModelDesc*)linear;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    LinearModel* linear = (LinearModel*)model;
    if (!linear->input || !linear->factor || !linear->offset || !linear->output)
        return -EINVAL;
    *linear->output = (*linear->input * *linear->factor) + *linear->offset;
    *model_time = stop_time;
    return 0;
}
