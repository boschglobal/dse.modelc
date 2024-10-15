// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>

/*
The properties and behaviour of this model are designed to be adjusted
at runtime via environment variables:
    * COUNTER_NAME - the name of the counter signal to increment.
    * COUNTER_VALUE - the initial value of the counter signal.
    * SIMBUS_LOGLEVEL - the log level (quiet=5, notice=4, info=2)
*/

typedef struct {
    ModelDesc        model;
    ModelSignalIndex counter;
} ExtendedModelDesc;

ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Identify the counter this model will increment. */
    char* counter_name =
        model_expand_vars((ModelDesc*)m, "${COUNTER_NAME:-counter_A}");
    m->counter = signal_index((ModelDesc*)m, "data", counter_name);
    if (m->counter.scalar == NULL)
        log_fatal("Signal not found (%s)", counter_name);
    free(counter_name);

    /* Set initial values. */
    char* counter_value =
        model_expand_vars((ModelDesc*)m, "${COUNTER_VALUE:-0}");
    *(m->counter.scalar) = atoi(counter_value);
    free(counter_value);

    /* Return the extended object. */
    return (ModelDesc*)m;
}

int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    if (m->counter.scalar == NULL) return -EINVAL;
    *(m->counter.scalar) += 1;

    const char* signal_name = m->counter.sv->signal[m->counter.signal];
    log_notice("counter %s = %f", signal_name, *(m->counter.scalar));
    log_info("counter %s = %f", signal_name, *(m->counter.scalar));

    *model_time = stop_time;
    return 0;
}
