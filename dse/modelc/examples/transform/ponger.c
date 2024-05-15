// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stddef.h>
#include <dse/modelc/model.h>

int model_step(ModelDesc* m, double* model_time, double stop_time)
{
    ModelSignalIndex ping = m->index(m, "data", "ping");
    ModelSignalIndex pong = m->index(m, "data", "pong");
    if (ping.scalar == NULL) return -EINVAL;
    if (pong.scalar == NULL) return -EINVAL;

    *(pong.scalar) = *(ping.scalar);

    *model_time = stop_time;
    return 0;
}
