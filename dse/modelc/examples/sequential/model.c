// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


typedef struct {
    SignalVector* sv;
    uint32_t      index;
    uint8_t*      buffer;
    uint32_t      buffer_size;
} BinarySignalDesc;

typedef struct {
    ModelDesc model;
    /* Scalar Signal Pointers. */
    struct {
        double* counter;
    } scalars;
    /* Binary Signal Indexes. */
    struct {
        BinarySignalDesc message;
    } binary;
} ExtendedModelDesc;


static inline double* _index_scalar(
    ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx.scalar;
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
    m->scalars.counter = _index_scalar(m, "scalar", "counter");
    m->binary.message = _index_binary(m, "binary", "message");

    /* Set initial values. */
    m->binary.message.buffer = calloc(50, sizeof(char));
    m->binary.message.buffer_size = 50;

    /* Return the extended object. */
    return (ModelDesc*)m;
}


static inline int _format_message(BinarySignalDesc* b, uint32_t uid, int v)
{
    return snprintf(
        (char*)b->buffer, b->buffer_size, "inst:%u count is %d", uid, v);
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Print the binary signal. */
    uint8_t* buffer;
    uint8_t* p_msg;
    size_t   len;
    signal_read(m->binary.message.sv, m->binary.message.index, &buffer, &len);
    p_msg = buffer;
    while (p_msg && ((p_msg - buffer) < (int32_t)len)) {
        log_info("Message (%s) : %s",
            m->binary.message.sv->signal[m->binary.message.index], p_msg);
        p_msg += strlen((char*)p_msg) + 1;
    }
    /* Scalar signals. */
    *(m->scalars.counter) += 1;

    /* Binary signals. */
    len = _format_message(
        &(m->binary.message), model->mi->uid, (int)*(m->scalars.counter));
    if (len >= (m->binary.message.buffer_size - 1)) {
        m->binary.message.buffer = realloc(m->binary.message.buffer, len + 1);
        m->binary.message.buffer_size = len + 1;
        _format_message(
            &(m->binary.message), model->mi->uid, (int)*(m->scalars.counter));
    }
    signal_reset(m->binary.message.sv, m->binary.message.index);
    signal_append(m->binary.message.sv, m->binary.message.index,
        m->binary.message.buffer, strlen((char*)m->binary.message.buffer) + 1);

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    if (m->binary.message.buffer) free(m->binary.message.buffer);
}
