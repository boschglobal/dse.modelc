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
} BinarySignalDesc;

typedef struct {
    ModelDesc model;
    /* Binary Signal Indexes. */
    struct {
        BinarySignalDesc message;
    } binary;
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
    m->binary.message = _index_binary(m, "binary", "message");

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    uint8_t* buffer;
    size_t   len;
    signal_read(m->binary.message.sv, m->binary.message.index, &buffer, &len);
    log_info("[%s@%0.6f] (len=%zu)", model->mi->name, *model_time, len);

    size_t offset = 0;
    while (offset < len) {
        const char* msg = (const char*)buffer + offset;
        const char* end = memchr(msg, '\0', len - offset);
        if (end == NULL) break;

        size_t msg_len = (size_t)(end - msg);
        log_info("[%s@%0.6f] Message: %s (len=%zu)", model->mi->name,
            *model_time, msg, msg_len);
        offset += msg_len + 1;
    }

    *model_time = stop_time;
    return 0;
}
