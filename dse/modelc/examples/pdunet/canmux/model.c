// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdio.h>
#include <dse/clib/collections/vector.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/modelc/pdunet.h>


typedef struct {
    ModelDesc model;
    /* Tx signals (one PDU, multiplexed by Mode). */
    struct {
        double* mode; /* 0 = Temp view, 1 = Electrical view. */
        double* tempA;
        double* tempB;
        double* voltage;
        double* current;
    } tx;
    /* Rx signals (loopback). */
    struct {
        double* mode;
        double* tempA;
        double* tempB;
        double* voltage;
        double* current;
    } rx;
} MuxModelDesc;


static inline double* _sig(MuxModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx.scalar;
}


ModelDesc* model_create(ModelDesc* model)
{
    MuxModelDesc* m = calloc(1, sizeof(MuxModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    m->tx.mode = _sig(m, "scalar_vector", "Mode");
    m->tx.tempA = _sig(m, "scalar_vector", "TempA");
    m->tx.tempB = _sig(m, "scalar_vector", "TempB");
    m->tx.voltage = _sig(m, "scalar_vector", "Voltage");
    m->tx.current = _sig(m, "scalar_vector", "Current");

    m->rx.mode = _sig(m, "scalar_vector", "ModeRx");
    m->rx.tempA = _sig(m, "scalar_vector", "TempARx");
    m->rx.tempB = _sig(m, "scalar_vector", "TempBRx");
    m->rx.voltage = _sig(m, "scalar_vector", "VoltageRx");
    m->rx.current = _sig(m, "scalar_vector", "CurrentRx");

    /* Seed sensor values; only Mode rotates over time. */
    *m->tx.tempA = 21.5;
    *m->tx.tempB = 22.0;
    *m->tx.voltage = 12.40;
    *m->tx.current = 1.25;

    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    MuxModelDesc* m = (MuxModelDesc*)model;

    /* Rotate Mode 0 <-> 1 every 30 ms. PDU is scheduled on 10 ms,
       step is 0.5 ms -> 20 model steps per PDU cycle, 60 per Mode flip. */
    uint32_t ticks = (uint32_t)(*model_time / model->sim->step_size);
    uint32_t pdu_ticks = ticks / 20; /* one tick per PDU cycle */
    *m->tx.mode = (double)((pdu_ticks / 3) % 2);

    log_info("step[%06f]: Tx Mode=%u "
             "(TempA=%.1f TempB=%.1f V=%.2f I=%.2f)  |  "
             "Rx Mode=%u TempA=%.1f TempB=%.1f V=%.2f I=%.2f",
        *model_time, (unsigned)*m->tx.mode, *m->tx.tempA, *m->tx.tempB,
        *m->tx.voltage, *m->tx.current, (unsigned)*m->rx.mode, *m->rx.tempA,
        *m->rx.tempB, *m->rx.voltage, *m->rx.current);

    *model_time = stop_time;
    return 0;
}
