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
        double* alive;
        double* alive_rx;
        double* foo;
        double* bar;
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
    m->signals.alive = _index_signal(m, "scalar_vector", "Alive");
    m->signals.alive_rx = _index_signal(m, "scalar_vector", "AliveRx");
    m->signals.foo = _index_signal(m, "scalar_vector", "FOO");
    m->signals.bar = _index_signal(m, "scalar_vector", "BAR");

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    PduNetModelDesc* m = (PduNetModelDesc*)model;

    /* Update signals. */
    double sample[4] = { 42, 24, 22, 44 };
    *m->signals.foo = sample[(int)(*m->signals.alive_rx) % 4];

    log_info("step[%06f]: Alive=%f, AliveRx=%f, FOO=%f, BAR=%f", *model_time,
        *m->signals.alive, *m->signals.alive_rx, *m->signals.foo,
        *m->signals.bar);

    /* Advance the model time. */
    *model_time = stop_time;
    return 0;
}
