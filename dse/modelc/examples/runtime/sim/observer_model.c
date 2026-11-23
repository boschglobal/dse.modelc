// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct ObserverModel {
    ModelDesc model;
    double*   output;
    NCODEC*   ncodec;
    uint32_t  message_id;
} ObserverModel;


ModelDesc* model_create(ModelDesc* model)
{
    ObserverModel* observer = calloc(1, sizeof(ObserverModel));
    observer->model = *model;
    observer->output = signal_index(model, "observer_vector", "output").scalar;
    ModelSignalIndex pdu = signal_index(model, "pdu_vector", "message");
    observer->ncodec = signal_codec(pdu.sv, pdu.signal);
    observer->message_id = 1000;
    if (!observer->output || pdu.binary == NULL || observer->ncodec == NULL) {
        free(observer);
        return NULL;
    }
    return (ModelDesc*)observer;
}

int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ObserverModel* observer = (ObserverModel*)model;
    char           payload[96];
    int length = snprintf(payload, sizeof(payload), "[%0.6f]output=%f",
        stop_time, *observer->output);
    if (length < 0 || (size_t)length >= sizeof(payload)) return -EOVERFLOW;

    NCodecPdu pdu = {
        .id = ++observer->message_id,
        .payload = (uint8_t*)payload,
        .payload_len = (uint32_t)length + 1,
        .ecu_id = 2,
        .swc_id = 5,
    };
    ncodec_truncate(observer->ncodec);
    if (ncodec_write(observer->ncodec, &pdu) < 0) return -EIO;
    ncodec_flush(observer->ncodec);

    *model_time = stop_time;
    return 0;
}
