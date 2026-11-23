// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>
#include <assert.h>
#include <errno.h>


typedef struct RuntimeNCodecModel {
    ModelDesc model;
    NCODEC*   ncodec;
    double*   received_mask;
    double*   received_count;
    bool      sent;
} RuntimeNCodecModel;


ModelDesc* model_create(ModelDesc* model)
{
    RuntimeNCodecModel* runtime_model = calloc(1, sizeof(RuntimeNCodecModel));
    runtime_model->model = *model;

    ModelSignalIndex pdu = signal_index(model, "ncodec_vector", "pdu");
    ModelSignalIndex received_mask =
        signal_index(model, "scalar_vector", "received_mask");
    ModelSignalIndex received_count =
        signal_index(model, "scalar_vector", "received_count");
    runtime_model->ncodec = signal_codec(pdu.sv, pdu.signal);
    runtime_model->received_mask = received_mask.scalar;
    runtime_model->received_count = received_count.scalar;
    if (pdu.binary == NULL || runtime_model->ncodec == NULL ||
        runtime_model->received_mask == NULL ||
        runtime_model->received_count == NULL) {
        free(runtime_model);
        return NULL;
    }
    return (ModelDesc*)runtime_model;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    RuntimeNCodecModel* runtime_model = (RuntimeNCodecModel*)model;
    uint32_t            received_mask = 0;
    uint32_t            received_count = 0;
    NCodecPdu           pdu = {};
    int                 len;

    while ((len = ncodec_read(runtime_model->ncodec, &pdu)) >= 0) {
        (void)len;
        assert(pdu.id == 2001);
        assert(pdu.swc_id == 4);
        received_mask |= 2U;
        received_count++;
    }
    *runtime_model->received_mask = received_mask;
    *runtime_model->received_count = received_count;

    if (!runtime_model->sent) {
        static const uint8_t payload[] = "target-pdu";
        ncodec_truncate(runtime_model->ncodec);
        if (ncodec_write(
                runtime_model->ncodec, &(NCodecPdu){
                                           .id = 1001,
                                           .payload = (uint8_t*)payload,
                                           .payload_len = sizeof(payload),
                                           .ecu_id = 1,
                                           .swc_id = 2,
                                       }) < 0) {
            return -EIO;
        }
        ncodec_flush(runtime_model->ncodec);
        runtime_model->sent = true;
    }

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    (void)model;
}
