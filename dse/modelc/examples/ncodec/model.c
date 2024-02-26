// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>


typedef struct {
    ModelDesc   model;
    /* Network Codec reference. */
    NCODEC*     nc;
    /* Model Instance values. */
    const char* node_id;
    uint8_t     counter;
} ExtendedModelDesc;


static inline NCODEC* _index(ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = m->model.index((ModelDesc*)m, v, s);
    if (idx.binary == NULL) log_fatal("Signal not found (%s:%s)", v, s);

    NCODEC* nc = idx.sv->codec(idx.sv, idx.signal);
    if (nc == NULL) log_fatal("NCodec object not available (%s:%s)", v, s);

    return nc;
}


static void _adjust_node_id(NCODEC* nc, const char* node_id)
{
    ncodec_config(nc, (struct NCodecConfigItem){
                          .name = "node_id",
                          .value = node_id,
                      });
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index the Network Codec. */
    m->nc = _index(m, "binary", "can");

    /* Configure the Network Codec. */
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem nci = ncodec_stat(m->nc, &i);
        if (strcmp(nci.name, "node_id") == 0) {
            m->node_id = nci.value;
            break;
        }
    }
    if (m->node_id == NULL) {
        m->node_id = model_instance_annotation((ModelDesc*)m, "node_id");
        if (m->node_id == NULL) log_fatal("No node_id configuration found!");
        ncodec_config(m->nc, (struct NCodecConfigItem){
                                 .name = "node_id",
                                 .value = m->node_id,
                             });
    }

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Message RX - spoof the node_id to avoid RX filtering. */
    _adjust_node_id(m->nc, "49");
    while (1) {
        NCodecCanMessage msg = {};
        int              len = ncodec_read(m->nc, &msg);
        if (len < 0) break;
        log_info("RX (%04x): %s", msg.frame_id, msg.buffer);
    }
    _adjust_node_id(m->nc, m->node_id);

    /* Message TX. */
    char msg[100] = "Hello World!";
    snprintf(msg + strlen(msg), sizeof(msg), " from node_id=%s", m->node_id);
    struct NCodecCanMessage tx_msg = {
        .frame_id = ++m->counter + 1000, /* Simple frame_id: 1001, 1002 ... */
        .frame_type = CAN_EXTENDED_FRAME,
        .buffer = (uint8_t*)msg,
        .len = strlen(msg) + 1, /* Capture the NULL terminator. */
    };
    ncodec_truncate(m->nc);
    ncodec_write(m->nc, &tx_msg);
    ncodec_flush(m->nc);

    *model_time = stop_time;
    return 0;
}
