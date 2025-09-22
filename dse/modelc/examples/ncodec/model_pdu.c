// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>


typedef struct {
    ModelDesc   model;
    /* Network Codec reference. */
    NCODEC*     nc;
    /* Model Instance values. */
    const char* swc_id;
    const char* ecu_id;
    uint8_t     counter;
} ExtendedModelDesc;


static inline NCODEC* _index(ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.binary == NULL) log_fatal("Signal not found (%s:%s)", v, s);

    NCODEC* nc = signal_codec(idx.sv, idx.signal);
    if (nc == NULL) log_fatal("NCodec object not available (%s:%s)", v, s);

    return nc;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index the Network Codec. */
    m->nc = _index(m, "binary", "pdu");

    /* Configure the Network Codec. */
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem nci = ncodec_stat(m->nc, &i);
        if (strcmp(nci.name, "swc_id") == 0) {
            m->swc_id = nci.value;
            break;
        } else if (strcmp(nci.name, "ecu_id") == 0) {
            m->ecu_id = nci.value;
            break;
        }
    }
    if (m->swc_id == NULL) {
        m->swc_id = model_instance_annotation((ModelDesc*)m, "swc_id");
        if (m->swc_id == NULL) log_fatal("No swc_id configuration found!");
        ncodec_config(m->nc, (struct NCodecConfigItem){
                                 .name = "swc_id",
                                 .value = m->swc_id,
                             });
    }
    if (m->ecu_id == NULL) {
        m->ecu_id = model_instance_annotation((ModelDesc*)m, "ecu_id");
        if (m->ecu_id == NULL) log_fatal("No ecu_id configuration found!");
        ncodec_config(m->nc, (struct NCodecConfigItem){
                                 .name = "ecu_id",
                                 .value = m->ecu_id,
                             });
    }

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Message RX. */
    while (1) {
        NCodecPdu pdu = {};
        int       len = ncodec_read(m->nc, &pdu);
        if (len < 0) break;
        log_info("RX (%08x): %s", pdu.id, pdu.payload);
    }

    /* Message TX. */
    char msg[100] = "Hello World!";
    snprintf(msg + strlen(msg), sizeof(msg), " from swc_id=%s", m->swc_id);
    struct NCodecPdu tx_msg = {
        .id = ++m->counter + 1000, /* Simple frame_id: 1001, 1002 ... */
        .payload = (uint8_t*)msg,
        .payload_len = strlen(msg) + 1, /* Capture the NULL terminator. */
        .swc_id = 42,  /* Set swc_id to bypass Rx filtering. */
        // Transport: IP
        .transport_type = NCodecPduTransportTypeIp,
        .transport.ip_message = {
            // Ethernet
            .eth_dst_mac = 0x0000123456789ABC,
            .eth_src_mac = 0x0000CBA987654321,
            .eth_ethertype = 1,
            .eth_tci_pcp = 2,
            .eth_tci_dei = 3,
            .eth_tci_vid = 4,
            // IP: IPv6
            .ip_protocol = NCodecPduIpProtocolUdp,
            .ip_addr_type = NCodecPduIpAddrIPv6,
            .ip_addr.ip_v6 = {
                .src_addr = {1, 2, 3, 4, 5, 6, 7, 8},
                .dst_addr = {2, 2, 4, 4, 6, 6, 8, 8},
            },
            .ip_src_port = 4242,
            .ip_dst_port = 2424,
            // Socket Adapter: DoIP
            .so_ad_type = NCodecPduSoAdDoIP,
            .so_ad.do_ip = {
                .protocol_version = 1,
                .payload_type = 2,
            },
        },
    };
    ncodec_truncate(m->nc);
    ncodec_write(m->nc, &tx_msg);
    ncodec_flush(m->nc);

    *model_time = stop_time;
    return 0;
}
