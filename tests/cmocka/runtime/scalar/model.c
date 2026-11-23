// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model.h>
#include <errno.h>


ModelDesc* model_create(ModelDesc* model)
{
    return model;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ModelSignalIndex counter = signal_index(model, "scalar_vector", "counter");
    if (counter.scalar == NULL) return -EINVAL;
    *counter.scalar += 1.0;
    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    (void)model;
}
