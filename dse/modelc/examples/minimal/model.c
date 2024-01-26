// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stddef.h>
#include <dse/modelc/model.h>

int model_step(ModelDesc* m, double* model_time, double stop_time)
{
    ModelSignalIndex counter = m->index(m, "data", "counter");
    if (counter.scalar == NULL) return -EINVAL;
    *(counter.scalar) += 1;
    *model_time = stop_time;
    return 0;
}
