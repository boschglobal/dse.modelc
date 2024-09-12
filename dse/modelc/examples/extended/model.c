// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


typedef struct {
    ModelDesc model;
    /* Signal Pointers. */
    struct {
        double* counter;
        double* odd;
        double* even;
    } signals;
} ExtendedModelDesc;


static inline double* _index(ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx.scalar;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index the signals that are used by this model. */
    m->signals.counter = _index(m, "data", "counter");
    m->signals.odd = _index(m, "data", "odd");
    m->signals.even = _index(m, "data", "even");

    /* Set initial values. */
    *(m->signals.counter) = 42;
    *(m->signals.odd) = false;
    *(m->signals.even) = true;

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    *(m->signals.counter) += 1;
    *(m->signals.odd) = (int)(*(m->signals.counter)) % 2 ? true : false;
    *(m->signals.even) = (int)(*(m->signals.counter)) % 2 ? false : true;

    *model_time = stop_time;
    return 0;
}
