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
        double* sig_1;
        double* sig_2;
        double* sig_3;
        double* sig_4;
        double* sig_5;
        double* sig_6;
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
    m->signals.sig_1 = _index_signal(m, "scalar_vector", "SIG_1");
    m->signals.sig_2 = _index_signal(m, "scalar_vector", "SIG_2");
    m->signals.sig_3 = _index_signal(m, "scalar_vector", "SIG_3");
    m->signals.sig_4 = _index_signal(m, "scalar_vector", "SIG_4");
    m->signals.sig_5 = _index_signal(m, "scalar_vector", "SIG_5");
    m->signals.sig_6 = _index_signal(m, "scalar_vector", "SIG_6");

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    PduNetModelDesc* m = (PduNetModelDesc*)model;

    /* PDU is on a 5ms schedule, with 4ms deadline. Update signals on
    a 20ms schedule (40) which gives time for the I-PDUs to be transported
    (takes 2 schedule slots). */
    uint32_t ticks = *model_time / model->sim->step_size;
    if ((ticks % 40) == 0) {
        *m->signals.sig_1 += 1;
        *m->signals.sig_2 += 2;
        *m->signals.sig_3 += 3;
    }
    log_info("step[%06f]: 1=%u, 2=%u, 3=%u, 4=%u, 5=%u, 6=%u", *model_time,
        (uint16_t)*m->signals.sig_1, (uint16_t)*m->signals.sig_2,
        (uint16_t)*m->signals.sig_3, (uint16_t)*m->signals.sig_4,
        (uint16_t)*m->signals.sig_5, (uint16_t)*m->signals.sig_6);

    /* Advance the model time. */
    *model_time = stop_time;
    return 0;
}
