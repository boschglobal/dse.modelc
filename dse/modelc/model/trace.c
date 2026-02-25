// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/frame.h>
#include <dse/ncodec/interface/pdu.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x)      ((void)x)
#define ARRAY_SIZE(x)  (sizeof(x) / sizeof(x[0]))

#define NCT_BUFFER_LEN 2000
#define NCT_ENVVAR_LEN 100
#define NCT_ID_LEN     100
#define NCT_KEY_LEN    20


typedef struct NCodecTraceData {
    const char* model_inst_name;
    double*     simulation_time;
    char        identifier[NCT_ID_LEN];
    /* Filters. */
    bool        wildcard;
    HashMap     filter;
} NCodecTraceData;


static const char* _get_codec_config(NCodecInstance* nc, const char* name)
{
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem ci = ncodec_stat((void*)nc, &i);
        if (ci.name && (strcmp(ci.name, name) == 0)) {
            return ci.value;
        }
    }
    return NULL;
}


/* CAN Trace
   --------- */
static void _trace_can_log(
    NCodecInstance* nc, NCodecMessage* m, const char* direction)
{
    NCodecTraceData*  td = nc->private;
    NCodecCanMessage* msg = m;
    static char       b[NCT_BUFFER_LEN];
    static char       identifier[NCT_ID_LEN];

    /* Setup bus identifier (on first call). */
    if (strlen(td->identifier) == 0) {
        snprintf(td->identifier, NCT_ID_LEN, "%s:%s:%s",
            _get_codec_config(nc, "bus_id"), _get_codec_config(nc, "node_id"),
            _get_codec_config(nc, "interface_id"));
    }

    /* Filter the message. */
    if (td->wildcard == false) {
        char key[NCT_KEY_LEN];
        snprintf(key, NCT_KEY_LEN, "%u", msg->frame_id);
        if (hashmap_get(&td->filter, key) == NULL) return;
    }

    /* Format and write the log. */
    memset(b, 0, NCT_BUFFER_LEN);
    if (strcmp(direction, "RX") == 0) {
        snprintf(identifier, NCT_ID_LEN, "%d:%d:%d", msg->sender.bus_id,
            msg->sender.node_id, msg->sender.interface_id);
    } else {
        strncpy(identifier, td->identifier, NCT_ID_LEN);
    }
    if (msg->len <= 16) {
        // Short form log.
        for (uint32_t i = 0; i < msg->len; i++) {
            if (i && (i % 8 == 0)) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", msg->buffer[i]);
        }
    } else {
        // Long form log.
        for (uint32_t i = 0; i < msg->len; i++) {
            if (strlen(b) > NCT_BUFFER_LEN) break;
            if (i % 32 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), "\n ");
            }
            if (i % 8 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", msg->buffer[i]);
        }
    }
    log_notice("(%s) %.6f [%s] %s %02x %d %d :%s", td->model_inst_name,
        *td->simulation_time, identifier, direction, msg->frame_id,
        msg->frame_type, msg->len, b);
}

static void _trace_can_read(NCODEC* nc, NCodecMessage* m)
{
    _trace_can_log((NCodecInstance*)nc, m, "RX");
}

static void _trace_can_write(NCODEC* nc, NCodecMessage* m)
{
    _trace_can_log((NCodecInstance*)nc, m, "TX");
}


/* PDU Trace
   --------- */
static const char* _lpdu_status_str(NCodecPduFlexrayLpduStatus v)
{
    const char* _t[] = {
        [NCodecPduFlexrayLpduStatusNone] = "None",
        [NCodecPduFlexrayLpduStatusTransmitted] = "Transmitted",
        [NCodecPduFlexrayLpduStatusNotTransmitted] = "NotTransmitted",
        [NCodecPduFlexrayLpduStatusReceived] = "Received",
        [NCodecPduFlexrayLpduStatusNotReceived] = "NotReceived",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _channel_tcvr_state_str(NCodecPduFlexrayTransceiverState v)
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
static const char* _channel_poc_state_str(NCodecPduFlexrayPocState v)
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
static const char* _channel_poc_command_str(NCodecPduFlexrayPocCommand v)
{
    const char* _t[] = {
        [NCodecPduFlexrayCommandNone] = "None",
        [NCodecPduFlexrayCommandConfig] = "Config",
        [NCodecPduFlexrayCommandReady] = "Ready",
        [NCodecPduFlexrayCommandWakeup] = "Wakeup",
        [NCodecPduFlexrayCommandRun] = "Run",
        [NCodecPduFlexrayCommandAllSlots] = "AllSlots",
        [NCodecPduFlexrayCommandHalt] = "Halt",
        [NCodecPduFlexrayCommandFreeze] = "Freeze",
        [NCodecPduFlexrayCommandAllowColdstart] = "AllowColdstart",
        [NCodecPduFlexrayCommandNop] = "Nop",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _config_op_str(NCodecPduFlexrayConfigOp v)
{
    const char* _t[] = {
        [NCodecPduFlexrayLpduConfigSet] = "Set",
        [NCodecPduFlexrayLpduConfigFrameTableSet] = "Frame Table Set",
        [NCodecPduFlexrayLpduConfigFrameTableMerge] = "Frame Table Merge",
        [NCodecPduFlexrayLpduConfigFrameTableDelete] = "Frame Table Delete",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _config_bit_rate_str(NCodecPduFlexrayBitrate v)
{
    const char* _t[] = {
        [NCodecPduFlexrayBitrateNone] = "None",
        [NCodecPduFlexrayBitrate10] = "10",
        [NCodecPduFlexrayBitrate5] = "5",
        [NCodecPduFlexrayBitrate2_5] = "2.5",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _config_channel_str(NCodecPduFlexrayChannel v)
{
    const char* _t[] = {
        [NCodecPduFlexrayChannelNone] = "None",
        [NCodecPduFlexrayChannelA] = "A",
        [NCodecPduFlexrayChannelB] = "B",
        [NCodecPduFlexrayChannelAB] = "AB",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _config_lpdu_dir_str(NCodecPduFlexrayDirection v)
{
    const char* _t[] = {
        [NCodecPduFlexrayDirectionNone] = "None",
        [NCodecPduFlexrayDirectionRx] = "Rx",
        [NCodecPduFlexrayDirectionTx] = "Tx",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}
static const char* _config_lpdu_txmode_str(NCodecPduFlexrayTransmitMode v)
{
    const char* _t[] = {
        [NCodecPduFlexrayTransmitModeNone] = "None",
        [NCodecPduFlexrayTransmitModeContinuous] = "Continuous",
        [NCodecPduFlexrayTransmitModeSingleShot] = "SingleShot",
    };
    if (v > ARRAY_SIZE(_t)) {
        return NULL;
    } else {
        return _t[v];
    }
}

static void _trace_pdu_log(
    NCodecInstance* nc, NCodecMessage* m, const char* direction)
{
    NCodecTraceData* td = nc->private;
    NCodecPdu*       pdu = m;
    static char      b[NCT_BUFFER_LEN];
    static char      identifier[NCT_ID_LEN];

    /* Setup bus identifier (on first call). */
    if (strlen(td->identifier) == 0) {
        snprintf(td->identifier, NCT_ID_LEN, "%s:%s",
            _get_codec_config(nc, "swc_id"), _get_codec_config(nc, "ecu_id"));
    }

    /* Filter the message. */
    if (td->wildcard == false) {
        char key[NCT_KEY_LEN];
        snprintf(key, NCT_KEY_LEN, "%u", pdu->id);
        if (hashmap_get(&td->filter, key) == NULL) return;
    }

    /* Format and write the log. */
    memset(b, 0, NCT_BUFFER_LEN);
    if (strcmp(direction, "RX") == 0) {
        snprintf(identifier, NCT_ID_LEN, "%d:%d", pdu->swc_id, pdu->ecu_id);
    } else {
        strncpy(identifier, td->identifier, NCT_ID_LEN);
    }
    if (pdu->payload_len <= 16) {
        // Short form log.
        for (uint32_t i = 0; i < pdu->payload_len; i++) {
            if (i && (i % 8 == 0)) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", pdu->payload[i]);
        }
    } else {
        // Long form log.
        for (uint32_t i = 0; i < pdu->payload_len; i++) {
            if (strlen(b) > NCT_BUFFER_LEN) break;
            if (i % 32 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), "\n ");
            }
            if (i % 8 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", pdu->payload[i]);
        }
    }
    log_notice("(%s) %.6f [%s] %s %02x %d :%s", td->model_inst_name,
        *td->simulation_time, identifier, direction, pdu->id, pdu->payload_len,
        b);

    // Transport
    switch (pdu->transport_type) {
    case NCodecPduTransportTypeCan: {
        // CAN
        log_notice(
            "    CAN:    frame_format=%d  frame_type=%d  interface_id=%d  "
            "network_id=%d",
            pdu->transport.can_message.frame_format,
            pdu->transport.can_message.frame_type,
            pdu->transport.can_message.interface_id,
            pdu->transport.can_message.network_id);
    } break;
    case NCodecPduTransportTypeIp: {
        // ETH
        log_notice("    ETH:    src_mac=%016x  dst_mac=%016x",
            pdu->transport.ip_message.eth_src_mac,
            pdu->transport.ip_message.eth_dst_mac);
        log_notice("    ETH:    ethertype=%04x  tci_pcp=%02x  tci_dei=%02x  "
                   "tci_vid=%04x",
            pdu->transport.ip_message.eth_ethertype,
            pdu->transport.ip_message.eth_tci_pcp,
            pdu->transport.ip_message.eth_tci_dei,
            pdu->transport.ip_message.eth_tci_vid);

        // IP
        switch (pdu->transport.ip_message.ip_addr_type) {
        case NCodecPduIpAddrIPv4: {
            log_notice("    IP:    src_addr=%08x  dst_addr=%08x",
                pdu->transport.ip_message.ip_addr.ip_v4.src_addr,
                pdu->transport.ip_message.ip_addr.ip_v4.dst_addr);
        } break;
        case NCodecPduIpAddrIPv6: {
            log_notice(
                "    IP:     src_addr=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[0],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[1],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[2],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[3],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[4],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[5],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[6],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[7]);
            log_notice(
                "    IP:     dst_addr=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[0],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[1],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[2],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[3],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[4],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[5],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[6],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[7]);
        } break;
        default:
            break;
        }
        log_notice("    IP:     src_port=%04x  dst_port=%04x  proto=%d",
            pdu->transport.ip_message.ip_src_port,
            pdu->transport.ip_message.ip_dst_port,
            pdu->transport.ip_message.ip_protocol);

        // Socket Adapter
        switch (pdu->transport.ip_message.so_ad_type) {
        case NCodecPduSoAdDoIP: {
            log_notice("    DOIP:   protocol_version=%d  payload_type=%d",
                pdu->transport.ip_message.so_ad.do_ip.protocol_version,
                pdu->transport.ip_message.so_ad.do_ip.payload_type);
        } break;
        case NCodecPduSoAdSomeIP: {
            log_notice("    SOMEIP: protocol_version=%d  interface_version=%d",
                pdu->transport.ip_message.so_ad.some_ip.protocol_version,
                pdu->transport.ip_message.so_ad.some_ip.interface_version);
            log_notice("    SOMEIP: request_id=%d  return_code=%d",
                pdu->transport.ip_message.so_ad.some_ip.request_id,
                pdu->transport.ip_message.so_ad.some_ip.return_code);
            log_notice("    SOMEIP: message_type=%d  message_id=%d  length=%d",
                pdu->transport.ip_message.so_ad.some_ip.message_type,
                pdu->transport.ip_message.so_ad.some_ip.message_id,
                pdu->transport.ip_message.so_ad.some_ip.length);
        } break;
        default:
            break;
        }
    } break;
    case NCodecPduTransportTypeFlexray: {
        NCodecPduFlexrayNodeIdentifier ni = pdu->transport.flexray.node_ident;

        switch (pdu->transport.flexray.metadata_type) {
        case NCodecPduFlexrayMetadataTypeConfig: {
            NCodecPduFlexrayConfig c = pdu->transport.flexray.metadata.config;
            log_notice("    FlexRay: (%u:%u:%u) Config vcn=%u", ni.node.ecu_id,
                ni.node.cc_id, ni.node.swc_id, c.vcn_count);
            log_notice("        operation=%s", _config_op_str(c.operation));
            log_notice("        macrotick_per_cycle=%u", c.macrotick_per_cycle);
            log_notice("        microtick_per_cycle=%u", c.microtick_per_cycle);
            log_notice("        network_idle_start=%u", c.network_idle_start);
            log_notice("        static_slot_length=%u", c.static_slot_length);
            log_notice("        static_slot_count=%u", c.static_slot_count);
            log_notice("        static_slot_payload_length=%u",
                c.static_slot_payload_length);
            log_notice("        minislot_length=%u", c.minislot_length);
            log_notice("        minislot_count=%u", c.minislot_count);
            log_notice("        bit_rate=%s", _config_bit_rate_str(c.bit_rate));
            log_notice("        channel_enable=%s",
                _config_channel_str(c.channel_enable));
            log_notice("        coldstart_node=%s",
                c.coldstart_node ? "true" : "false");
            log_notice("        sync_node=%s", c.sync_node ? "true" : "false");
            log_notice("        Frame Table:");
            for (size_t i = 0; i < c.frame_config.count; i++) {
                NCodecPduFlexrayLpduConfig lc = c.frame_config.table[i];
                log_notice("            [%u] LPDU %04x base=%u rep=%u dir=%s "
                           "ch=%s tx_mode=%s inhibit_null=%s",
                    i, lc.slot_id, lc.base_cycle, lc.cycle_repetition,
                    _config_lpdu_dir_str(lc.direction),
                    _config_channel_str(lc.channel),
                    _config_lpdu_txmode_str(lc.transmit_mode),
                    lc.inhibit_null ? "true" : "false");
            }
        } break;
        case NCodecPduFlexrayMetadataTypeStatus: {
            NCodecPduFlexrayStatus s = pdu->transport.flexray.metadata.status;
            log_notice("    FlexRay: (%u:%u:%u) Status cycle=%u ma=%u; CH0 "
                       "tcvr=%s poc_state=%s poc_cmd=%s",
                ni.node.ecu_id, ni.node.cc_id, ni.node.swc_id, s.cycle,
                s.macrotick, _channel_tcvr_state_str(s.channel[0].tcvr_state),
                _channel_poc_state_str(s.channel[0].poc_state),
                _channel_poc_command_str(s.channel[0].poc_command));
        } break;
        case NCodecPduFlexrayMetadataTypeLpdu: {
            NCodecPduFlexrayLpdu l = pdu->transport.flexray.metadata.lpdu;
            log_notice(
                "    FlexRay: (%u:%u:%u) LPDU %04x index=%u, len=%u, status=%s",
                ni.node.ecu_id, ni.node.cc_id, ni.node.swc_id, pdu->id,
                l.frame_config_index, pdu->payload_len,
                _lpdu_status_str(l.status));
        } break;
        default:
            break;
        }
    } break;
    default:
        break;
    }
}

static void _trace_pdu_read(NCODEC* nc, NCodecMessage* m)
{
    _trace_pdu_log((NCodecInstance*)nc, m, "RX");
}

static void _trace_pdu_write(NCODEC* nc, NCodecMessage* m)
{
    _trace_pdu_log((NCodecInstance*)nc, m, "TX");
}

static void _trace_log(NCODEC* nc, NCodecTraceLogLevel level, const char* msg)
{
    UNUSED(nc);
    if (level >= __log_level__) __log2console(level, NULL, 0, "%s", msg);
}


/* Trace Configuration
   ------------------- */
DLL_PRIVATE void ncodec_trace_configure(
    NCodecInstance* nc, ModelInstanceSpec* mi, bool force)
{
    bool        type_can = false;
    bool        type_pdu = false;
    const char* codec_type = _get_codec_config(nc, "type");
    if (codec_type == NULL) return;
    if (strcmp(codec_type, "frame") == 0) type_can = true;
    if (strcmp(codec_type, "pdu") == 0) type_pdu = true;

    /* Install the log function. */
    if (getenv("NCODEC_TRACE_LOG") != NULL || force == true) {
        nc->trace.log = _trace_log;
    }

    /* Query the environment variable. */
    char env_name[NCT_ENVVAR_LEN];
    if (type_can) {
        snprintf(env_name, NCT_ENVVAR_LEN, "NCODEC_TRACE_%s_%s",
            _get_codec_config(nc, "bus"), _get_codec_config(nc, "bus_id"));
    } else if (type_pdu) {
        snprintf(env_name, NCT_ENVVAR_LEN, "NCODEC_TRACE_PDU_%s",
            _get_codec_config(nc, "swc_id"));
    } else {
        return;
    }
    for (int i = 0; env_name[i]; i++)
        env_name[i] = toupper(env_name[i]);
    char* filter = getenv(env_name);
    if (filter == NULL) return;

    /* Setup the trace data. */
    ModelInstancePrivate* mip = mi->private;
    AdapterModel*         am = mip->adapter_model;
    NCodecTraceData*      td = calloc(1, sizeof(NCodecTraceData));

    td->model_inst_name = mi->name;
    td->simulation_time = &am->model_time;
    hashmap_init(&td->filter);
    if (strcmp(filter, "*") == 0) {
        td->wildcard = true;
        log_notice("    <wildcard> (all frames)");
    } else {
        char* _filter = strdup(filter);
        char* _saveptr = NULL;
        char* _idptr = strtok_r(_filter, ",", &_saveptr);
        while (_idptr) {
            int64_t _id = strtol(_idptr, NULL, 0);
            if (_id > 0) {
                char key[NCT_KEY_LEN];
                snprintf(key, NCT_KEY_LEN, "%u", (uint32_t)_id);
                hashmap_set_long(&td->filter, key, true);
                log_notice("    %02x", _id);
            }
            _idptr = strtok_r(NULL, ",", &_saveptr);
        }
        free(_filter);
    }

    /* Install the trace. */
    if (type_can) {
        nc->trace.write = _trace_can_write;
        nc->trace.read = _trace_can_read;
        nc->private = td;
    } else if (type_pdu) {
        nc->trace.write = _trace_pdu_write;
        nc->trace.read = _trace_pdu_read;
        nc->private = td;
    }
}


DLL_PRIVATE void ncodec_trace_destroy(NCodecInstance* nc)
{
    if (nc->private) {
        NCodecTraceData* td = nc->private;
        hashmap_destroy(&td->filter);
        free(td);
        nc->private = NULL;
    }
}
