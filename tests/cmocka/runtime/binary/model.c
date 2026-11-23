// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dse/modelc/model.h>


#define RUNTIME_BINARY_TARGET_MESSAGE  "target-message"
#define RUNTIME_BINARY_VIRTUAL_MESSAGE "virtual-message"


static bool _has_both_messages(const uint8_t* buffer, size_t buffer_len)
{
    const size_t target_len = sizeof(RUNTIME_BINARY_TARGET_MESSAGE);
    const size_t virtual_len = sizeof(RUNTIME_BINARY_VIRTUAL_MESSAGE);
    if (buffer == NULL || buffer_len != target_len + virtual_len) return false;

    return (memcmp(buffer, RUNTIME_BINARY_TARGET_MESSAGE, target_len) == 0 &&
               memcmp(buffer + target_len, RUNTIME_BINARY_VIRTUAL_MESSAGE,
                   virtual_len) == 0) ||
           (memcmp(buffer, RUNTIME_BINARY_VIRTUAL_MESSAGE, virtual_len) == 0 &&
               memcmp(buffer + virtual_len, RUNTIME_BINARY_TARGET_MESSAGE,
                   target_len) == 0);
}


typedef struct RuntimeBinaryModel {
    ModelDesc     model;
    SignalVector* message_sv;
    uint32_t      message_index;
    double*       received;
    bool          sent;
} RuntimeBinaryModel;


ModelDesc* model_create(ModelDesc* model)
{
    RuntimeBinaryModel* runtime_model = calloc(1, sizeof(RuntimeBinaryModel));
    memcpy(&runtime_model->model, model, sizeof(ModelDesc));

    ModelSignalIndex message = signal_index(model, "binary_vector", "message");
    ModelSignalIndex received =
        signal_index(model, "scalar_vector", "target_received");
    runtime_model->message_sv = message.sv;
    runtime_model->message_index = message.signal;
    runtime_model->received = received.scalar;
    if (message.binary == NULL || runtime_model->received == NULL) {
        free(runtime_model);
        return NULL;
    }
    return (ModelDesc*)runtime_model;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    RuntimeBinaryModel* runtime_model = (RuntimeBinaryModel*)model;
    uint8_t*            buffer = NULL;
    size_t              buffer_len = 0;

    signal_read(runtime_model->message_sv, runtime_model->message_index,
        &buffer, &buffer_len);
    *runtime_model->received =
        _has_both_messages(buffer, buffer_len) ? 1.0 : 0.0;
    signal_reset(runtime_model->message_sv, runtime_model->message_index);
    if (!runtime_model->sent) {
        signal_append(runtime_model->message_sv, runtime_model->message_index,
            (void*)RUNTIME_BINARY_TARGET_MESSAGE,
            sizeof(RUNTIME_BINARY_TARGET_MESSAGE));
        runtime_model->sent = true;
    }

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    (void)model;
}
