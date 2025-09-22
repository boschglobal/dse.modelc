// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/frame.h>
#include <dse/ncodec/interface/pdu.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/controller/model_private.h>


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


/* Trace Configuration
   ------------------- */
DLL_PRIVATE void ncodec_trace_configure(
    NCodecInstance* nc, ModelInstanceSpec* mi)
{
    bool        type_can = false;
    bool        type_pdu = false;
    const char* codec_type = _get_codec_config(nc, "type");
    if (codec_type == NULL) return;
    if (strcmp(codec_type, "frame") == 0) type_can = true;
    if (strcmp(codec_type, "pdu") == 0) type_pdu = true;

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
