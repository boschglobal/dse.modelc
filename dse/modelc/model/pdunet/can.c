// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model/pdunet/network.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


void pdunet_can_parse_network(PduNetworkDesc* net)
{
    net->network.transport_type = NCodecPduTransportTypeCan;
}


void pdunet_can_parse_pdu(PduItem* pdu, void* n)
{
    assert(pdu);
    assert(n);
    YamlNode* md = dse_yaml_find_node((YamlNode*)n, "metadata/can");
    if (md) {
        pdunet_can_parse_pdu_metadata(pdu, md);
    }
}


void pdunet_can_config(PduNetworkDesc* net)
{
    assert(net);
    assert(net->ncodec);
    assert(net->network.transport_type == NCodecPduTransportTypeCan);

    log_notice("PDU Net: CAN Network Config: %s", net->name);

    for (size_t i = 0; i < vector_len(&net->pdus); i++) {
        PduItem* pdu = vector_at(&net->pdus, i, NULL);
        if (pdu == NULL) continue;
        if (pdu->container.id != 0) continue; /* Container I-PDU. */
        const char* dir_str = (pdu->dir == PduDirectionTx)   ? "Tx"
                              : (pdu->dir == PduDirectionRx) ? "Rx"
                                                             : "None";
        const char* container_str =
            (pdu->container.header != HeaderFormatNone) ? " [Container]" : "";
        log_notice("PDU Net:   CAN: PDU[%u] id=0x%03X, len=%u, dir=%s, "
                   "name=%s%s",
            i, pdu->id, (unsigned)pdu->length, dir_str, pdu->name,
            container_str);
    }
}


void pdunet_can_lpdu_tx(PduNetworkDesc* net)
{
    assert(net);
    assert(net->ncodec);
    assert(net->network.transport_type == NCodecPduTransportTypeCan);

    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
        if (pdu == NULL) continue;
        assert(pdu->pdu);
        if (pdu->pdu->dir != PduDirectionTx) continue;
        if (pdu->pdu->container.id != 0) continue; /* Container I-PDU. */
        if (pdu->needs_tx == false) continue;

        NCodecPduCanMessageMetadata can_meta = { 0 };
        if (pdu->pdu->metadata.config) {
            NCodecPduCanMessageMetadata* pdu_config = pdu->pdu->metadata.config;
            can_meta = *pdu_config;
        }

        ncodec_write(
            net->ncodec, &(struct NCodecPdu){
                             .id = pdu->pdu->id,
                             .payload = pdu->ncodec.pdu.payload,
                             .payload_len = pdu->ncodec.pdu.payload_len,
                             .transport_type = NCodecPduTransportTypeCan,
                             .transport.can_message = can_meta,
                         });
        pdu->needs_tx = false;

        log_debug("  CAN: Tx[%u] id=0x%03X, len=%u", i, pdu->pdu->id,
            (unsigned)pdu->ncodec.pdu.payload_len);
    }
}


void pdunet_can_lpdu_rx(PduNetworkDesc* net)
{
    while (1) {
        NCodecPdu nc_pdu = {};
        if (ncodec_read(net->ncodec, &nc_pdu) < 0) break;
        if (nc_pdu.transport_type != NCodecPduTransportTypeCan) continue;

        for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
            PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
            if (pdu == NULL) continue;
            assert(pdu->pdu);
            if (pdu->pdu->dir != PduDirectionRx) continue;
            if (pdu->pdu->container.id != 0) continue; /* Container I-PDU. */
            if (pdu->pdu->id != nc_pdu.id) continue;

            size_t len = nc_pdu.payload_len;
            if (len > pdu->ncodec.pdu.payload_len) {
                len = pdu->ncodec.pdu.payload_len;
            }
            uint8_t* payload = NULL;
            vector_at(&(net->matrix.payload), pdu->matrix.pdu_idx, &payload);
            memcpy(payload, nc_pdu.payload, len);
            pdu->update_signals = true;

            log_debug(
                "  CAN: Rx[%u] id=0x%03X, len=%u", i, nc_pdu.id, (unsigned)len);
        }
    }
}


void pdunet_can_parse_pdu_metadata(PduItem* pdu, YamlNode* md)
{
    assert(pdu);
    assert(md);

    if (pdu->metadata.config != NULL) free(pdu->metadata.config);
    pdu->metadata.config = calloc(1, sizeof(NCodecPduCanMessageMetadata));

    static const SchemaFieldMapSpec ff_map[] = {
        { "Base", NCodecPduCanFrameFormatBase },
        { "Extended", NCodecPduCanFrameFormatExtended },
        { "FdBase", NCodecPduCanFrameFormatFdBase },
        { "FdExtended", NCodecPduCanFrameFormatFdExtended },
        { NULL },
    };
    static const SchemaFieldMapSpec ft_map[] = {
        { "Data", NCodecPduCanFrameTypeData },
        { "Remote", NCodecPduCanFrameTypeRemote },
        { "Error", NCodecPduCanFrameTypeError },
        { "Overload", NCodecPduCanFrameTypeOverload },
        { NULL },
    };
    static const SchemaFieldSpec md_can_spec[] = {
        // clang-format off
        { U8, "frame_format", offsetof(NCodecPduCanMessageMetadata, frame_format), ff_map },
        { U8, "frame_type", offsetof(NCodecPduCanMessageMetadata, frame_type), ft_map },
        { U32, "interface_id", offsetof(NCodecPduCanMessageMetadata, interface_id) },
        { U32, "network_id", offsetof(NCodecPduCanMessageMetadata, network_id) },
        // clang-format on
    };
    schema_load_object(
        md, pdu->metadata.config, md_can_spec, ARRAY_SIZE(md_can_spec));
}
