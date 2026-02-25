// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model/pdunet/network.h>
#include <dse/clib/util/cleanup.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


static const char* _poc_state_str(NCodecPduFlexrayPocState v)
{
    const char* _t[] = {
        [NCodecPduFlexrayPocStateDefaultConfig] = "DefaultConfig",
        [NCodecPduFlexrayPocStateConfig] = "Config",
        [NCodecPduFlexrayPocStateReady] = "Ready",
        [NCodecPduFlexrayPocStateWakeup] = "Wakeup",
        [NCodecPduFlexrayPocStateStartup] = "Startup",
        [NCodecPduFlexrayPocStateNormalActive] = "NormalActive",
        [NCodecPduFlexrayPocStateNormalPassive] = "NormalPassive",
        [NCodecPduFlexrayPocStateHalt] = "Halt",
        [NCodecPduFlexrayPocStateFreeze] = "Freeze",
        [NCodecPduFlexrayPocStateUndefined] = "Undefined",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _tcvr_state_str(NCodecPduFlexrayTransceiverState v)
{
    const char* _t[] = {
        [NCodecPduFlexrayTransceiverStateNone] = "None",
        [NCodecPduFlexrayTransceiverStateNoPower] = "NoPower",
        [NCodecPduFlexrayTransceiverStateNoConnection] = "NoConnection",
        [NCodecPduFlexrayTransceiverStateNoSignal] = "NoSignal",
        [NCodecPduFlexrayTransceiverStateCAS] = "CAS",
        [NCodecPduFlexrayTransceiverStateWUP] = "WUP",
        [NCodecPduFlexrayTransceiverStateFrameSync] = "FrameSync",
        [NCodecPduFlexrayTransceiverStateFrameError] = "FrameError",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const double flexray_microtick[] = {
    [NCodecPduFlexrayBitrateNone] = 0,
    [NCodecPduFlexrayBitrate10] = 0.000000025,
    [NCodecPduFlexrayBitrate5] = 0.000000025,
    [NCodecPduFlexrayBitrate2_5] = 0.000000050,
};


void pdunet_flexray_parse_network(PduNetworkDesc* net)
{
    YamlNode* md = dse_yaml_find_node(net->doc, "spec/metadata/flexray");
    if (md) {
        net->network.transport_type = 4;
        pdunet_flexray_parse_network_metadata(net, md);
    }
}

void pdunet_flexray_parse_pdu(PduItem* pdu, void* n)
{
    assert(pdu);
    assert(n);
    YamlNode* md = dse_yaml_find_node((YamlNode*)n, "metadata/flexray");
    if (md) {
        pdunet_flexray_parse_pdu_metadata(pdu, md);
    }
}

static const char* _frdir[] = {
    [NCodecPduFlexrayDirectionNone] = "None",
    [NCodecPduFlexrayDirectionRx] = "Rx",
    [NCodecPduFlexrayDirectionTx] = "Tx",
};

void pdunet_flexray_config(PduNetworkDesc* net)
{
    assert(net);
    assert(net->ncodec);
    assert(net->network.transport_type == NCodecPduTransportTypeFlexray);
    assert(net->network.metadata.config);

    log_notice("PDU Net: FlexRay Network Config: %s", net->name);

    /* Setup config with frame table. */
    CLEANUP_P(void, _ft) =
        calloc(vector_len(&net->pdus), sizeof(NCodecPduFlexrayLpduConfig));
    NCodecPduFlexrayLpduConfig* frame_table = _ft;
    for (size_t i = 0; i < vector_len(&net->pdus); i++) {
        PduItem* pdu = vector_at(&net->pdus, i, NULL);
        if (pdu == NULL || pdu->metadata.config == NULL) continue;
        NCodecPduFlexrayLpduConfig* pdu_config = pdu->metadata.config;
        pdu_config->index.frame_table = i;
        frame_table[i] = *pdu_config;

        log_notice(
            "PDU Net:   FlexRay: PDU[%u] id=%u, slot=%u, dir=%s, name=%s", i,
            pdu->id, pdu_config->slot_id, _frdir[pdu_config->direction],
            pdu->name);
    }
    NCodecPduFlexrayConfig config =
        *(NCodecPduFlexrayConfig*)net->network.metadata.config;
    config.frame_config.table = frame_table;
    config.frame_config.count = vector_len(&net->pdus);

    /* Write config to NCodec. */
    ncodec_write(net->ncodec, &(struct NCodecPdu){
        .transport_type = NCodecPduTransportTypeFlexray,
        .transport.flexray = {
            .metadata_type = NCodecPduFlexrayMetadataTypeConfig,
            .metadata.config = config,
        },
    });

    /* Allocate and configure LPDU metadata. */
    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
        if (pdu == NULL) continue;
        NCodecPduFlexrayLpduConfig* pdu_config = pdu->pdu->metadata.config;
        assert(pdu_config);
        if (pdu->ncodec.metadata.lpdu == NULL) {
            pdu->ncodec.metadata.lpdu = calloc(1, sizeof(NCodecPduFlexrayLpdu));
        }
        ((NCodecPduFlexrayLpdu*)pdu->ncodec.metadata.lpdu)->frame_config_index =
            pdu_config->index.frame_table;
    }

    /* Calculate stateful info. */
    net->network.vtable.flexray.cycle_time =
        (config.microtick_per_cycle * flexray_microtick[config.bit_rate]) /
        net->schedule.step_size;
    log_notice("PDU Net:   FlexRay: cycle_time=%f",
        net->network.vtable.flexray.cycle_time * net->schedule.step_size);
}

void pdunet_flexray_lpdu_tx(PduNetworkDesc* net)
{
    assert(net);
    assert(net->ncodec);
    assert(net->network.transport_type == NCodecPduTransportTypeFlexray);
    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
        if (pdu == NULL) continue;
        assert(pdu->pdu);
        assert(pdu->ncodec.metadata.lpdu);
        NCodecPduFlexrayLpduConfig* pdu_config = pdu->pdu->metadata.config;
        assert(pdu_config);
        NCodecPduFlexrayLpdu* lpdu = pdu->ncodec.metadata.lpdu;
        switch (pdu->pdu->dir) {
        case PduDirectionTx:
            /* Write the Tx PDU with payload. */
            if (pdu->needs_tx == true) {
                lpdu->status = NCodecPduFlexrayLpduStatusNotTransmitted;
                ncodec_write(net->ncodec,
                    &(struct NCodecPdu){ .id = pdu_config->slot_id,
                        .payload = pdu->ncodec.pdu.payload,
                        .payload_len = pdu->ncodec.pdu.payload_len,
                        .transport_type = NCodecPduTransportTypeFlexray,
                        .transport.flexray = {
                            .metadata_type = NCodecPduFlexrayMetadataTypeLpdu,
                            .metadata.lpdu = *lpdu,
                        } });
                pdu->needs_tx = false;
            }
            break;
        case PduDirectionRx:
            /* Write the Rx PDU setting state to not received. */
            if (lpdu->status != NCodecPduFlexrayLpduStatusNotReceived) {
                lpdu->status = NCodecPduFlexrayLpduStatusNotReceived;
                ncodec_write(net->ncodec,
                    &(struct NCodecPdu){ .id = pdu_config->slot_id,
                        //.payload_len = pdu->ncodec.pdu.payload_len,
                        .transport_type = NCodecPduTransportTypeFlexray,
                        .transport.flexray = {
                            .metadata_type = NCodecPduFlexrayMetadataTypeLpdu,
                            .metadata.lpdu = *lpdu,
                        } });
            }
            break;
        default:
            break;
        }
        if (pdu->pdu->dir != PduDirectionTx) continue;
    }
}


void pdunet_flexray_lpdu_rx(PduNetworkDesc* net)
{
    while (1) {
        NCodecPdu nc_pdu = {};
        if (ncodec_read(net->ncodec, &nc_pdu) < 0) break;
        if (nc_pdu.transport_type != NCodecPduTransportTypeFlexray) continue;
        switch (nc_pdu.transport.flexray.metadata_type) {
        case NCodecPduFlexrayMetadataTypeStatus:
            NCodecPduFlexrayStatus s = nc_pdu.transport.flexray.metadata.status;
            log_debug("  FlexRay: State channel[0]: poc_state=%s, "
                      "tcvr_state=%s, cycle=%u, macrotick=%u (net cycle=%u)",
                _poc_state_str(s.channel[0].poc_state),
                _tcvr_state_str(s.channel[0].tcvr_state), s.cycle, s.macrotick,
                net->network.vtable.flexray.cycle);

            if (net->network.vtable.flexray.cycle != s.cycle) {
                /* Set network offset according to position in new cycle. */
                NCodecPduFlexrayConfig* config =
                    (NCodecPduFlexrayConfig*)net->network.metadata.config;
                double previous_offset = net->schedule.epoch_offset;

                /* Normalised values (to/from step_size). */
                uint32_t cycle_time = (config->microtick_per_cycle *
                                          flexray_microtick[config->bit_rate]) /
                                      net->schedule.step_size;
                uint32_t offset = net->schedule.simulation_time % cycle_time;
                net->schedule.epoch_offset = offset * net->schedule.step_size;
                net->network.vtable.flexray.cycle = s.cycle;
                if (previous_offset != net->schedule.epoch_offset) {
                    log_info("Epoch shift: %f -> %f (cycle=%u, macrotick=%u)",
                        previous_offset, net->schedule.epoch_offset, s.cycle,
                        s.macrotick);
                }
            }
            continue;
        case NCodecPduFlexrayMetadataTypeLpdu:
            break;
        default:
            continue;
        }

        for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
            // TODO: Speedup: use pdu_config->index.lpdu_table as index to
            // payload? May need support in NCodec. Similar to
            // nc_pdu.transport.flexray.metadata.lpdu.frame_config_index and
            // would give speedup. Want direct lookup.

            PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
            if (pdu == NULL) continue;
            assert(pdu->pdu);
            if (pdu->pdu->dir != PduDirectionRx) continue;
            NCodecPduFlexrayLpduConfig* pdu_config = pdu->pdu->metadata.config;
            assert(pdu_config);
            if (pdu_config->index.frame_table !=
                nc_pdu.transport.flexray.metadata.lpdu.frame_config_index)
                continue;
            /* Update the LPDU status (will trigger Tx loop). */
            NCodecPduFlexrayLpdu* lpdu = pdu->ncodec.metadata.lpdu;
            if (lpdu) {
                lpdu->status = nc_pdu.transport.flexray.metadata.lpdu.status;
            }
            /* Process the payload. */
            size_t len = nc_pdu.payload_len;
            if (len > pdu->ncodec.pdu.payload_len) {
                len = pdu->ncodec.pdu.payload_len;
            }
            uint8_t* payload = NULL;
            vector_at(&(net->matrix.payload), pdu->matrix.pdu_idx, &payload);
            memcpy(payload, nc_pdu.payload, len);
            pdu->update_signals = true;

            log_debug("  FlexRay: Rx[%u] slot=%u", i, pdu_config->slot_id);
        }
    }
}

void pdunet_flexray_parse_network_metadata(PduNetworkDesc* net, YamlNode* md)
{
    assert(net);
    assert(md);
    assert(net->network.transport_type == NCodecPduTransportTypeFlexray);

    if (net->network.metadata.config != NULL) {
        free(net->network.metadata.config);
    }
    net->network.metadata.config = calloc(1, sizeof(NCodecPduFlexrayConfig));

    static const SchemaFieldMapSpec br_map[] = {
        { "BR10Mbps", NCodecPduFlexrayBitrate10 },
        { "BR5Mbps", NCodecPduFlexrayBitrate5 },
        { "BR2_5Mbps", NCodecPduFlexrayBitrate2_5 },
        { NULL },
    };
    static const SchemaFieldMapSpec ch_map[] = {
        { "A", NCodecPduFlexrayChannelA },
        { "B", NCodecPduFlexrayChannelB },
        { "AB", NCodecPduFlexrayChannelAB },
        { NULL },
    };
    static const SchemaFieldSpec md_fr_spec[] = {
        // clang-format off
        { B, "inhibit_null_frames", offsetof(NCodecPduFlexrayConfig, inhibit_null_frames) }, // NOLINT
        { U16, "macrotick_per_cycle", offsetof(NCodecPduFlexrayConfig, macrotick_per_cycle) }, // NOLINT
        { U32, "microtick_per_cycle", offsetof(NCodecPduFlexrayConfig, microtick_per_cycle) }, // NOLINT
        { U16, "network_idle_start", offsetof(NCodecPduFlexrayConfig, network_idle_start) }, // NOLINT
        { U16, "static_slot_length", offsetof(NCodecPduFlexrayConfig, static_slot_length) }, // NOLINT
        { U16, "static_slot_count", offsetof(NCodecPduFlexrayConfig, static_slot_count) }, // NOLINT
        { U8, "minislot_length", offsetof(NCodecPduFlexrayConfig, minislot_length) }, // NOLINT
        { U16, "minislot_count", offsetof(NCodecPduFlexrayConfig, minislot_count) }, // NOLINT
        { U32, "static_slot_payload_length", offsetof(NCodecPduFlexrayConfig, static_slot_payload_length) }, // NOLINT
        { U8, "bit_rate", offsetof(NCodecPduFlexrayConfig, bit_rate), br_map }, // NOLINT
        { U8, "channel_enable", offsetof(NCodecPduFlexrayConfig, channel_enable), ch_map }, // NOLINT
        { U8, "wakeup_channel_select", offsetof(NCodecPduFlexrayConfig, wakeup_channel_select), ch_map }, // NOLINT
        { B, "single_slot_enabled", offsetof(NCodecPduFlexrayConfig, single_slot_enabled) }, // NOLINT
        // clang-format on
    };
    schema_load_object(
        md, net->network.metadata.config, md_fr_spec, ARRAY_SIZE(md_fr_spec));
}

void pdunet_flexray_parse_pdu_metadata(PduItem* pdu, YamlNode* md)
{
    assert(pdu);
    assert(md);

    if (pdu->metadata.config != NULL) free(pdu->metadata.config);
    pdu->metadata.config = calloc(1, sizeof(NCodecPduFlexrayLpduConfig));

    static const SchemaFieldMapSpec dir_map[] = {
        { "Rx", NCodecPduFlexrayDirectionRx },
        { "Tx", NCodecPduFlexrayDirectionTx },
        { NULL },
    };
    static const SchemaFieldMapSpec ch_map[] = {
        { "A", NCodecPduFlexrayChannelA },
        { "B", NCodecPduFlexrayChannelB },
        { "AB", NCodecPduFlexrayChannelAB },
        { NULL },
    };
    static const SchemaFieldMapSpec mode_map[] = {
        { "Continuous", NCodecPduFlexrayTransmitModeContinuous },
        { "SingleShot", NCodecPduFlexrayTransmitModeSingleShot },
        { NULL },
    };
    static const SchemaFieldSpec md_fr_spec[] = {
        // clang-format off
            { U16, "slot_id", offsetof(NCodecPduFlexrayLpduConfig , slot_id) },
            { U8, "payload_length", offsetof(NCodecPduFlexrayLpduConfig , payload_length) },
            { U8, "cycle_repetition", offsetof(NCodecPduFlexrayLpduConfig , cycle_repetition) },
            { U8, "base_cycle", offsetof(NCodecPduFlexrayLpduConfig , base_cycle) },
            { U8, "direction", offsetof(NCodecPduFlexrayLpduConfig , direction), dir_map },
            { U8, "channel", offsetof(NCodecPduFlexrayLpduConfig , channel), ch_map },
            { U8, "transmit_mode", offsetof(NCodecPduFlexrayLpduConfig , transmit_mode), mode_map },
            { B, "inhibit_null", offsetof(NCodecPduFlexrayLpduConfig , inhibit_null) },
        // clang-format on
    };
    schema_load_object(
        md, pdu->metadata.config, md_fr_spec, ARRAY_SIZE(md_fr_spec));
}
