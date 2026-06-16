// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdio.h>
#include <dse/clib/collections/vector.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/modelc/pdunet.h>
#include <dse/ncodec/codec.h>


typedef struct {
    ModelDesc model;
    /* Signals. */
    struct {
        double* counter;
        double* counter_rx;
        double* speed;
        double* speed_rx;
    } signals;
} PduNetModelDesc;


static inline double* _index_signal(
    PduNetModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx.scalar;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    PduNetModelDesc* m = calloc(1, sizeof(PduNetModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index signals used by this model. */
    m->signals.counter = _index_signal(m, "scalar_vector", "Counter");
    m->signals.counter_rx = _index_signal(m, "scalar_vector", "CounterRx");
    m->signals.speed = _index_signal(m, "scalar_vector", "Speed");
    m->signals.speed_rx = _index_signal(m, "scalar_vector", "SpeedRx");

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    PduNetModelDesc* m = (PduNetModelDesc*)model;

    /* Update signals: derive speed from received counter. */
    double sample[4] = { 10, 20, 30, 40 };
    *m->signals.speed = sample[(int)(*m->signals.counter_rx) % 4];

    log_info("step[%06f]: Counter=%f, CounterRx=%f, Speed=%f, SpeedRx=%f",
        *model_time, *m->signals.counter, *m->signals.counter_rx,
        *m->signals.speed, *m->signals.speed_rx);

    /* Advance the model time. */
    *model_time = stop_time;
    return 0;
}
