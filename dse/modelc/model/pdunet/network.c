// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/modelc/model/pdunet/network.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/schema.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


uint32_t pdunet_checksum(const uint8_t* payload, size_t len)
{
    if (payload == NULL) return 0;

    // FNV-1a hash (http://www.isthe.com/chongo/tech/comp/fnv/)
    uint32_t csum = 2166136261UL; /* FNV_OFFSET 32 bit */
    for (size_t i = 0; i < len; ++i) {
        csum = csum ^ payload[i];
        csum = csum * 16777619UL; /* FNV_PRIME 32 bit */
    }
    return csum;
}


void pdunet_schedule(PduNetworkDesc* net)
{
    for (size_t pdu_idx = 0; pdu_idx < vector_len(&net->matrix.pdu);
        pdu_idx++) {
        PduObject* o = vector_at(&(net->matrix.pdu), pdu_idx, NULL);

        bool    schedule_skip = true; /* Signals are not calculated. */
        int32_t epoch_offset =
            (net->schedule.step_size)
                ? net->schedule.epoch_offset / net->schedule.step_size
                : 0;
        if (o->schedule.interval) {
            int32_t base = epoch_offset + o->schedule.phase;
            log_trace("Schedule TX: PDU %u @ %u: base=%d, epoch_offset=%d, "
                      "phase=%u, interval=%u",
                o->pdu->id, net->schedule.simulation_time, base, epoch_offset,
                o->schedule.phase, o->schedule.interval);
            if (base >= 0 && net->schedule.simulation_time >= (uint32_t)base) {
                uint32_t delta = net->schedule.simulation_time - base;
                if (delta % o->schedule.interval == 0) {
                    /* The schedule will fire, force recalculation of payload
                    to ensure PDU are sent _even_ if signals are unchanged
                    after signal calculation (skip==false, default).*/
                    log_trace("Schedule TX: PDU %u: trigger", o->pdu->id);
                    schedule_skip = false;
                    o->checksum = 0;
                }
            }
        } else {
            /* No schedule, always calculate signals (and send PDU's if the
            resultant payload has changed). */
            schedule_skip = false;
        }

        /* Apply the skip value to associated signals in the matrix. */
        bool* skip = (bool*)vector_at(
            &net->matrix.signal.skip, o->matrix.range.offset, NULL);
        for (size_t i = 0; i < o->matrix.range.count; i++) {
            skip[i] = schedule_skip;
        }
    }
}


PduNetworkNCodecVTable pdunet_network_factory(PduNetworkDesc* net)
{
    YamlNode* md = dse_yaml_find_node(net->doc, "spec/metadata/flexray");
    if (md) {
        net->network.transport_type = NCodecPduTransportTypeFlexray;
        return (PduNetworkNCodecVTable){
            .parse_network = pdunet_flexray_parse_network,
            .parse_pdu = pdunet_flexray_parse_pdu,
            .config = pdunet_flexray_config,
            .lpdu_tx = pdunet_flexray_lpdu_tx,
            .lpdu_rx = pdunet_flexray_lpdu_rx,
        };
    }

    return (PduNetworkNCodecVTable){ 0 };
}


static void* _object_match_handler(ModelInstanceSpec* mi, void* object)
{
    UNUSED(mi);
    return object;  // YamlNode
}


void pdunet_parse_pdus(PduNetworkDesc* net, SchemaObject* object)
{
    uint32_t index = 0;
    do {
        YamlNode* n = schema_object_enumerator(
            net->mi, object, "spec/pdus", &index, _object_match_handler);
        if (n == NULL) break;
        PduItem pdu = pdunet_pdu_generator(net, n);
        if (pdu.id == 0) continue;
        vector_push(&net->pdus, &pdu);
    } while (1);
}


PduItem pdunet_pdu_generator(PduNetworkDesc* net, YamlNode* n)
{
    PduItem pdu = { id : 0 };

    static const SchemaFieldMapSpec dir_map[] = {
        { "Rx", PduDirectionRx },
        { "Tx", PduDirectionTx },
        { NULL },
    };
    static const SchemaFieldSpec spec[] = {
        // clang-format off
        { S, "pdu", offsetof(PduItem, name) },
        { U32, "id", offsetof(PduItem, id) }, // Indicates successful parsing.
        { U32, "length", offsetof(PduItem, length) },
        { U8, "dir", offsetof(PduItem, dir), dir_map },
        { D, "schedule/phase", offsetof(PduItem, schedule.phase) },
        { D, "schedule/interval", offsetof(PduItem, schedule.interval) },
        // clang-format on
    };
    schema_load_object(n, &pdu, spec, ARRAY_SIZE(spec));
    pdunet_load_lua_func(n, "functions/encode", &pdu.lua.encode);
    pdunet_load_lua_func(n, "functions/decode", &pdu.lua.decode);

    // Metadata.
    if (net->network.vtable.parse_pdu) {
        net->network.vtable.parse_pdu(&pdu, n);
    }

    // Parse spec/pdus/signals.
    YamlNode* sigs = dse_yaml_find_node(n, "signals");
    if (sigs) {
        pdu.signals = vector_make(sizeof(PduSignalItem), 10, NULL);
        for (uint32_t i = 0; i < hashlist_length(&sigs->sequence); i++) {
            YamlNode*     sig = hashlist_at(&sigs->sequence, i);
            PduSignalItem signal = pdunet_signal_generator(net->mi, sig, &pdu);
            if (signal.name == NULL) continue;
            vector_push(&pdu.signals, &signal);
        }
    }

    // Return the object.
    return pdu;
}


PduSignalItem pdunet_signal_generator(
    ModelInstanceSpec* mi, YamlNode* n, PduItem* pdu)
{
    UNUSED(mi);

    PduSignalItem signal = { .factor = NAN,
        .offset = NAN,
        .min = NAN,
        .max = NAN,
        .start_bit = 0xffff };

    static const SchemaFieldSpec spec[] = {
        // clang-format off
        { S, "signal", offsetof(PduSignalItem, name) },
        // Encoding parameters.
        { U16, "encoding/start", offsetof(PduSignalItem, start_bit) },
        { U16, "encoding/length", offsetof(PduSignalItem, length_bits) },
        { D, "encoding/factor", offsetof(PduSignalItem, factor) },
        { D, "encoding/offset", offsetof(PduSignalItem, offset) },
        { D, "encoding/min", offsetof(PduSignalItem, min) },
        { D, "encoding/max", offsetof(PduSignalItem, max) },
        // clang-format on
    };
    schema_load_object(n, &signal, spec, ARRAY_SIZE(spec));

    bool is_valid = true;
    if (signal.factor == 0.0) {
        if (__log_level__ != LOG_QUIET)
            log_error("Invalid signal encoding: factor=0.0 (%s)", signal.name);
        is_valid = false;
    }
    if (signal.start_bit == 0xffff) {
        if (__log_level__ != LOG_QUIET)
            log_error("Invalid signal encoding: start not specified (%s)",
                signal.name);
        is_valid = false;
    }
    if (signal.length_bits == 0) {
        if (__log_level__ != LOG_QUIET)
            log_error("Invalid signal encoding: length not specified (%s)",
                signal.name);
        is_valid = false;
    }
    if (signal.start_bit > (pdu->length * 8)) {
        if (__log_level__ != LOG_QUIET)
            log_error("Invalid signal encoding: start is beyond payload "
                      "length (%s)",
                signal.name);
        is_valid = false;
    }
    if ((signal.start_bit + signal.length_bits) > (pdu->length * 8)) {
        if (__log_level__ != LOG_QUIET)
            log_error("Invalid signal encoding: length is beyond payload "
                      "length (%s)",
                signal.name);
        is_valid = false;
    }
    if (is_valid == false) {
        signal.name = NULL;  // Caller will reject.
    }

    pdunet_load_lua_func(n, "functions/encode", &signal.lua.encode);
    pdunet_load_lua_func(n, "functions/decode", &signal.lua.decode);

    return signal;
}


void pdunet_build_msm(PduNetworkDesc* net, const char* sv_name)
{
    assert(sv_name);
    assert(net);
    assert(net->mi);
    assert(net->mi->model_desc);

    for (SignalVector* sv = net->mi->model_desc->sv; sv && sv->name; sv++) {
        if (strcmp(sv->alias, sv_name) != 0) continue;
        MarshalMapSpec source = (MarshalMapSpec){
            .count = net->matrix.signal.count,
            .signal =
                (const char**)vector_at(&net->matrix.signal.name, 0, NULL),
            .scalar = (double*)vector_at(&net->matrix.signal.phys, 0, NULL),
        };
        MarshalMapSpec signal = (MarshalMapSpec){
            .name = sv->name,
            .count = sv->count,
            .signal = sv->signal,
            .scalar = sv->scalar,
        };
        errno = 0;
        MarshalSignalMap* msm =
            marshal_generate_signalmap(signal, source, NULL, false);
        if (errno == 0) {
            net->msm = calloc(2, sizeof(MarshalSignalMap));
            net->msm[0] = *msm; /* Shallow copy. */
        }
        free(msm);
        break;
    }
}


/**
pdunet_parse
============

Parameters
----------
net (PduNetworkDesc*)
: PduNetworkDesc object.
*/
static int _network_match_handler(ModelInstanceSpec* mi, SchemaObject* object)
{
    UNUSED(mi);

    PduNetworkDesc* net = object->data;
    net->doc = object->doc;
    net->network.vtable = pdunet_network_factory(net);

    // Parse metadata/name.
    dse_yaml_get_string(net->doc, "metadata/name", &net->name);
    log_notice("  Parsing network: %s", net->name);

    // Parse schedule.
    static const SchemaFieldSpec spec[] = {
        // clang-format off
        { D, "spec/schedule/epoch_offset", offsetof(PduNetworkDesc, schedule.epoch_offset) },
        // clang-format on
    };
    schema_load_object(net->doc, net, spec, ARRAY_SIZE(spec));

    // Parse spec/metadata.
    if (net->network.vtable.parse_network) {
        net->network.vtable.parse_network(net);
    }

    // Parse spec/pdus.
    pdunet_parse_pdus(net, object);

    return 1;
}

int pdunet_parse(PduNetworkDesc* net, SchemaLabel* labels)
{
    assert(net);
    assert(labels);
    if (net->mi == NULL) return EINVAL;

    // Construct a schema selector.
    size_t label_count = 0;
    for (SchemaLabel* l = labels; l->name != NULL; l++) {
        label_count++;
    }
    SchemaObjectSelector selector = {
        .kind = "Network",
        .labels = labels,
        .labels_len = label_count,
        .data = net,
    };

    // Parse the network.
    return schema_object_search(net->mi, &selector, _network_match_handler);
}


/**
pdunet_transform
================

Parameters
----------
net (PduNetworkDesc*)
: PduNetworkDesc object.
*/
int pdunet_transform(PduNetworkDesc* net, PduNetworkSortFunc sort)
{
    return pdunet_matrix_transform(net, sort);
}


/**
pdunet_configure
================

Parameters
----------
net (PduNetworkDesc*)
: PduNetworkDesc object.
*/
int pdunet_configure(PduNetworkDesc* net)
{
    assert(net);
    const char* name;
    int         rc = 0;

    rc = pdunet_lua_setup(net);
    if (rc != 0) return rc;

    assert(net);
    assert(net->mi);
    assert(net->mi->private);
    ModelInstancePrivate* mip = net->mi->private;
    lua_State*            L = mip->lua_state;

    pdunet_parse_network_functions(net);
    if (net->lua.global != NULL) {
        pdunet_lua_install_func(L, NULL, net->lua.global);
    }

    size_t pdu_count = vector_len(&net->pdus);
    for (size_t pdu_idx = 0; pdu_idx < pdu_count; pdu_idx++) {
        PduItem* p = vector_at(&net->pdus, pdu_idx, NULL);
        // PDU -> install lua func.
        name = pdunet_build_func_name(net, p, NULL, "encode");
        p->lua.encode_ref = pdunet_lua_install_func(L, name, p->lua.encode);
        name = pdunet_build_func_name(net, p, NULL, "decode");
        p->lua.decode_ref = pdunet_lua_install_func(L, name, p->lua.decode);

        for (size_t sig_idx = 0; sig_idx < vector_len(&p->signals); sig_idx++) {
            PduSignalItem* s = vector_at(&p->signals, sig_idx, NULL);
            // Signal -> install lua func.
            name = pdunet_build_func_name(net, p, s, "encode");
            s->lua.encode_ref = pdunet_lua_install_func(L, name, s->lua.encode);
            name = pdunet_build_func_name(net, p, s, "decode");
            s->lua.decode_ref = pdunet_lua_install_func(L, name, s->lua.decode);
        }
    }

    return rc;
}
