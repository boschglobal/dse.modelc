// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/ncodec/codec.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/controller/model_private.h>


#define NCT_BUFFER_LEN 2000
#define NCT_ENVVAR_LEN 100
#define NCT_BUSID_LEN  50
#define NCT_KEY_LEN    20


typedef struct NCodecTraceData {
    const char* model_inst_name;
    double*     simulation_time;
    char        bus_identifier[NCT_BUSID_LEN];
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

static void _trace_log(
    NCodecInstance* nc, NCodecMessage* m, const char* direction)
{
    NCodecTraceData*  td = nc->private;
    NCodecCanMessage* msg = m;
    static char       b[NCT_BUFFER_LEN];
    static char       bus_identifier[NCT_BUSID_LEN];

    /* Setup bus identifier (on first call). */
    if (strlen(td->bus_identifier) == 0) {
        snprintf(td->bus_identifier, NCT_BUSID_LEN, "%s:%s:%s",
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
        snprintf(bus_identifier, NCT_BUSID_LEN, "%d:%d:%d", msg->sender.bus_id,
            msg->sender.node_id, msg->sender.interface_id);
    } else {
        strncpy(bus_identifier, td->bus_identifier, NCT_BUSID_LEN);
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
        *td->simulation_time, bus_identifier, direction, msg->frame_id,
        msg->frame_type, msg->len, b);
}

static void _trace_read(NCODEC* nc, NCodecMessage* m)
{
    _trace_log((NCodecInstance*)nc, m, "RX");
}

static void _trace_write(NCODEC* nc, NCodecMessage* m)
{
    _trace_log((NCodecInstance*)nc, m, "TX");
}

DLL_PRIVATE void ncodec_trace_configure(
    NCodecInstance* nc, ModelInstanceSpec* mi)
{
    /* Query the environment variable. */
    char env_name[NCT_ENVVAR_LEN];
    snprintf(env_name, NCT_ENVVAR_LEN, "NCODEC_TRACE_%s_%s",
        _get_codec_config(nc, "bus"), _get_codec_config(nc, "bus_id"));
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
        char* _frameptr = strtok_r(_filter, ",", &_saveptr);
        while (_frameptr) {
            int64_t frame_id = strtol(_frameptr, NULL, 0);
            if (frame_id > 0) {
                char key[NCT_KEY_LEN];
                snprintf(key, NCT_KEY_LEN, "%u", (uint32_t)frame_id);
                hashmap_set_long(&td->filter, key, true);
                log_notice("    %02x", frame_id);
            }
            _frameptr = strtok_r(NULL, ",", &_saveptr);
        }
        free(_filter);
    }

    /* Install the trace. */
    nc->trace.write = _trace_write;
    nc->trace.read = _trace_read;
    nc->private = td;
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
