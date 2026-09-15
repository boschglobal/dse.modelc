// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
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
    /* Flow-control test parameters (envar controlled, see README). */
    uint32_t backpressure_size;     /* Inflate message to this size (bytes). */
    uint32_t backpressure_delay_ms; /* Sleep this long each step (ms). */
} ExtendedModelDesc;


/* Per-instance override (<INST>__<NAME>) falling back to a global <NAME>. */
static inline uint32_t _get_envar(ModelDesc* m, const char* n, uint32_t val)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "%s__%s", m->mi->name, n);
    for (size_t i = 0; i < strlen(buf); i++)
        buf[i] = toupper(buf[i]);
    if (getenv(buf)) {
        val = (uint32_t)atol(getenv(buf));
    } else {
        snprintf(buf, sizeof(buf), "%s", n);
        for (size_t i = 0; i < strlen(buf); i++)
            buf[i] = toupper(buf[i]);
        if (getenv(buf)) {
            val = (uint32_t)atol(getenv(buf));
        }
    }
    if (val) log_notice("  get_envar:%s=%u", buf, val);

    return val;
}


static inline void _sleep_ms(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000U);
#endif
}


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
    *(m->scalars.counter) = 42;
    m->binary.message.buffer = calloc(10, sizeof(char));
    m->binary.message.buffer_size = 10;

    /* Flow-control test parameters, disabled (0) unless set. */
    m->backpressure_size = _get_envar(model, "BACKPRESSURE_SIZE", 0);
    m->backpressure_delay_ms = _get_envar(model, "BACKPRESSURE_DELAY_MS", 0);

    /* Return the extended object. */
    return (ModelDesc*)m;
}


static inline int _format_message(BinarySignalDesc* b, int v)
{
    return snprintf((char*)b->buffer, b->buffer_size, "count is %d", v);
}

int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Simulate a slow consumer: delay before this step's message exchange. */
    if (m->backpressure_delay_ms) _sleep_ms(m->backpressure_delay_ms);

    /* Print the binary signal (truncate/preview only; payload may be large
       and is not guaranteed to be null-terminated when inflated). */
    uint8_t* buffer;
    size_t   len;
    signal_read(m->binary.message.sv, m->binary.message.index, &buffer, &len);
    int preview_len = (int)(len < 32 ? len : 32);
    log_info("Message (%s) : %.*s (len=%zu)",
        m->binary.message.sv->signal[m->binary.message.index], preview_len,
        (char*)buffer, len);

    /* Scalar signals. */
    *(m->scalars.counter) += 1;
    /* Binary signals. */
    uint32_t msg_size = m->backpressure_size;
    if (msg_size) {
        /* Inflate the message to stress send/recv buffering and flow
           control (payload content is otherwise not significant). */
        if (msg_size > m->binary.message.buffer_size) {
            m->binary.message.buffer = realloc(m->binary.message.buffer, msg_size);
            m->binary.message.buffer_size = msg_size;
        }
        int hdr_len = snprintf((char*)m->binary.message.buffer, msg_size,
            "count is %d ", (int)*(m->scalars.counter));
        if (hdr_len > 0 && (uint32_t)hdr_len < msg_size) {
            memset(m->binary.message.buffer + hdr_len, 'x',
                msg_size - (uint32_t)hdr_len);
        }
        signal_reset(m->binary.message.sv, m->binary.message.index);
        signal_append(m->binary.message.sv, m->binary.message.index,
            m->binary.message.buffer, msg_size);
    } else {
        len =
            _format_message(&(m->binary.message), (int)*(m->scalars.counter));
        if (len >= (m->binary.message.buffer_size - 1)) {
            m->binary.message.buffer =
                realloc(m->binary.message.buffer, len + 1);
            m->binary.message.buffer_size = len + 1;
            _format_message(
                &(m->binary.message), (int)*(m->scalars.counter));
        }
        signal_reset(m->binary.message.sv, m->binary.message.index);
        signal_append(m->binary.message.sv, m->binary.message.index,
            m->binary.message.buffer,
            strlen((char*)m->binary.message.buffer) + 1);
    }

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    if (m->binary.message.buffer) free(m->binary.message.buffer);
}
