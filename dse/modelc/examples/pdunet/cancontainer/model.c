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
        /* Container 0x300, periodic I-PDUs (10 ms). */
        double* c300_10ms_1;
        double* c300_10ms_2;
        double* c300_10ms_3;
        double* c300_10ms_1_rx;
        double* c300_10ms_2_rx;
        double* c300_10ms_3_rx;
        /* Standalone periodic L-PDU 0x310 (20 ms). */
        double* l310_20ms;
        double* l310_20ms_rx;
        /* Standalone on-change L-PDU 0x320. */
        double* l320_onchange;
        double* l320_onchange_rx;
        /* Container 0x330, non-periodic (on-change) I-PDUs. */
        double* c330_onchange_1;
        double* c330_onchange_2;
        double* c330_onchange_1_rx;
        double* c330_onchange_2_rx;
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
    m->signals.c300_10ms_1 = _index_signal(m, "scalar_vector", "C300_10ms_1");
    m->signals.c300_10ms_2 = _index_signal(m, "scalar_vector", "C300_10ms_2");
    m->signals.c300_10ms_3 = _index_signal(m, "scalar_vector", "C300_10ms_3");
    m->signals.c300_10ms_1_rx =
        _index_signal(m, "scalar_vector", "C300_10ms_1_Rx");
    m->signals.c300_10ms_2_rx =
        _index_signal(m, "scalar_vector", "C300_10ms_2_Rx");
    m->signals.c300_10ms_3_rx =
        _index_signal(m, "scalar_vector", "C300_10ms_3_Rx");
    m->signals.l310_20ms = _index_signal(m, "scalar_vector", "L310_20ms");
    m->signals.l310_20ms_rx = _index_signal(m, "scalar_vector", "L310_20ms_Rx");
    m->signals.l320_onchange =
        _index_signal(m, "scalar_vector", "L320_OnChange");
    m->signals.l320_onchange_rx =
        _index_signal(m, "scalar_vector", "L320_OnChange_Rx");
    m->signals.c330_onchange_1 =
        _index_signal(m, "scalar_vector", "C330_OnChange_1");
    m->signals.c330_onchange_2 =
        _index_signal(m, "scalar_vector", "C330_OnChange_2");
    m->signals.c330_onchange_1_rx =
        _index_signal(m, "scalar_vector", "C330_OnChange_1_Rx");
    m->signals.c330_onchange_2_rx =
        _index_signal(m, "scalar_vector", "C330_OnChange_2_Rx");

    *m->signals.l310_20ms = 7;
    *m->signals.l320_onchange = 9;
    *m->signals.c330_onchange_1 = 11;
    *m->signals.c330_onchange_2 = 12;

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    PduNetModelDesc* m = (PduNetModelDesc*)model;

    /* PDU is on a 10ms schedule. Update Tx signals on a 40ms schedule
    (every 4 PDU intervals) which gives time for the I-PDUs to be
    transported through the Container L-PDU loopback. */
    uint32_t ticks = *model_time / model->sim->step_size;
    if ((ticks % 80) == 0) {
        *m->signals.c300_10ms_1 += 1;
        *m->signals.c300_10ms_2 += 2;
        *m->signals.c300_10ms_3 += 3;
    }
    log_info("step[%06f]: C300 1=%u, 2=%u, 3=%u, 1Rx=%u, 2Rx=%u, 3Rx=%u",
        *model_time, (uint16_t)*m->signals.c300_10ms_1,
        (uint16_t)*m->signals.c300_10ms_2, (uint16_t)*m->signals.c300_10ms_3,
        (uint16_t)*m->signals.c300_10ms_1_rx,
        (uint16_t)*m->signals.c300_10ms_2_rx,
        (uint16_t)*m->signals.c300_10ms_3_rx);
    log_info("step[%06f]: L310 periodic tx=%u, rx=%u", *model_time,
        (uint16_t)*m->signals.l310_20ms, (uint16_t)*m->signals.l310_20ms_rx);
    log_info("step[%06f]: L320 onchange tx=%u, rx=%u", *model_time,
        (uint16_t)*m->signals.l320_onchange,
        (uint16_t)*m->signals.l320_onchange_rx);
    log_info("step[%06f]: C330 1=%u, 2=%u, 1Rx=%u, 2Rx=%u", *model_time,
        (uint16_t)*m->signals.c330_onchange_1,
        (uint16_t)*m->signals.c330_onchange_2,
        (uint16_t)*m->signals.c330_onchange_1_rx,
        (uint16_t)*m->signals.c330_onchange_2_rx);

    /* Advance the model time. */
    *model_time = stop_time;
    return 0;
}
