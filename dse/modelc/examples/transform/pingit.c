// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stddef.h>
#include <dse/modelc/model.h>

int model_step(ModelDesc* m, double* model_time, double stop_time)
{
    ModelSignalIndex ping = m->index(m, "data", "ping");
    if (ping.scalar == NULL) return -EINVAL;

    double val = *(ping.scalar);
    val = (val == 100) ? -100 : 100;

    *(ping.scalar) = val;
    *model_time = stop_time;
    return 0;
}
