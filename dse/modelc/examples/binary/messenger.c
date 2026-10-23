// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


typedef struct {
    double* scalar;
    double  start;
} ScalarSignalDesc;

typedef struct {
    SignalVector* sv;
    uint32_t      index;
    uint8_t       buffer[128];
} BinarySignalDesc;

typedef struct {
    ModelDesc model;
    /* Scalar Signal Pointers. */
    struct {
        ScalarSignalDesc counter;
    } scalars;
    /* Binary Signal Indexes. */
    struct {
        BinarySignalDesc message;
    } binary;
} ExtendedModelDesc;


static inline ScalarSignalDesc _index_scalar(
    ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    const char* start = signal_annotation(idx.sv, idx.signal, "start", NULL);
    ScalarSignalDesc ret = {
        .scalar = idx.scalar,
        .start = start ? atof(start) : 42,
    };
    return ret;
}


static inline BinarySignalDesc _index_binary(
    ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.binary == NULL) log_fatal("Signal not found (%s:%s)", v, s);

    BinarySignalDesc ret = {
        .sv = &(m->model.sv[idx.vector]),
        .index = idx.signal,
    };
    return ret;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index the signals that are used by this model. */
    size_t name_size = strlen(model->mi->name) + sizeof("_counter");
    char*  counter_name = malloc(name_size);
    snprintf(counter_name, name_size, "%s_counter", model->mi->name);
    m->scalars.counter = _index_scalar(m, "scalar", counter_name);
    free(counter_name);
    m->binary.message = _index_binary(m, "binary", "message");

    /* Set the inital values. */
    *(m->scalars.counter.scalar) = m->scalars.counter.start;

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Increment the counter. */
    *(m->scalars.counter.scalar) += 1;

    /* Produce the message. */
    snprintf((char*)m->binary.message.buffer, sizeof(m->binary.message.buffer),
        "%s count is %d", model->mi->name, (int)*(m->scalars.counter.scalar));
    size_t len = strlen((char*)m->binary.message.buffer);
    signal_reset(m->binary.message.sv, m->binary.message.index);
    signal_append(m->binary.message.sv, m->binary.message.index,
        m->binary.message.buffer, len + 1);

    /* Complete the step. */
    *model_time = stop_time;
    return 0;
}
