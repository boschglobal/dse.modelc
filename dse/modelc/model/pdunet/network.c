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
        if (o->pdu->container.id != 0) {
            /* This PDU is encapsulated in a Container PDU and will be
            updated by the schedule of that Container. */
            continue;
        }

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
                    /* This is a Container PDU, reset checksum for all
                    encapsulated PDUs. */
                    if (o->container.header != HeaderFormatNone) {
                        for (size_t i = 0;
                            i < vector_len(&o->container.pdu_list); i++) {
                            MPduItem* pi =
                                vector_at(&o->container.pdu_list, i, NULL);
                            assert(pi);
                            assert(pi->pdu);
                            pi->pdu->checksum = 0;
                        }
                    }
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
    static const SchemaFieldMapSpec header_map[] = {
        { "Static", HeaderFormatStatic },
        { "Short", HeaderFormatShort },
        { "Full", HeaderFormatFull },
        { NULL },
    };
    static const SchemaFieldSpec spec[] = {
        // clang-format off
        { S, "pdu", offsetof(PduItem, name) },
        { U32, "id", offsetof(PduItem, id) }, // Indicates successful parsing.
        { U32, "length", offsetof(PduItem, length) },
        { U8, "dir", offsetof(PduItem, dir), dir_map },
        { U8, "container/header", offsetof(PduItem, container.header), header_map },
        { U32, "container/id", offsetof(PduItem, container.id) },
        { U32, "container/priority", offsetof(PduItem, container.priority) },
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

    /* Locate the signal vector. */
    SignalVector* sv = NULL;
    for (SignalVector* _ = net->mi->model_desc->sv; _ && _->name; _++) {
        if (strcmp(_->alias, sv_name) != 0) continue;
        sv = _;
        break;
    }
    if (sv == NULL) return;
    MarshalMapSpec signal = (MarshalMapSpec){
        .name = sv->name,
        .count = sv->count,
        .signal = sv->signal,
        .scalar = sv->scalar,
    };

    /* Loop over matrix ranges and emit a MSM for each range. */
    Vector in = vector_make(sizeof(MarshalSignalMap), 0, NULL);
    Vector out = vector_make(sizeof(MarshalSignalMap), 0, NULL);
    for (size_t range_idx = 0; range_idx < vector_len(&net->matrix.range);
        range_idx++) {
        PduRange* r = vector_at(&net->matrix.range, range_idx, NULL);
        assert(r);
        MarshalMapSpec source = (MarshalMapSpec){
            .count = r->length,
            .signal = (const char**)vector_at(
                &net->matrix.signal.name, r->offset, NULL),
            .scalar =
                (double*)vector_at(&net->matrix.signal.phys, r->offset, NULL),
        };

        /* Build the MSM object for this range item. */
        errno = 0;
        MarshalSignalMap* msm =
            marshal_generate_signalmap(signal, source, NULL, false);
        if (errno != 0 || msm == NULL) {
            log_info(
                "Call to marshal_generate_signalmap() failed: errno=%d", errno);
            continue;
        }
        log_debug("MSM for range[%d]: dir=%u, offset=%u, length=%u", range_idx,
            r->dir, r->offset, r->length);

        /* Shallow copy the MSM object into a vector. */
        switch (r->dir) {
        case PduDirectionRx:
            vector_push(&in, msm);
            break;
        case PduDirectionTx:
            vector_push(&out, msm);
            break;
        default:
            break;
        }
        free(msm);
    }

    /* Add empty MSM object to each vector (creating an NTL). */
    vector_push(&in, &(MarshalSignalMap){});
    vector_push(&out, &(MarshalSignalMap){});

    /* Use the internal vector objects, which are NTLs. */
    net->msm.in = in.items;
    net->msm.out = out.items;
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
    int rc = pdunet_matrix_transform(net, sort);
    if (rc) return rc;

    pdunet_visit(net, NULL, pdunet_visit_setup_containers, NULL);
    return 0;
}


static int _sort_mpdu(const void* left, const void* right)
{
    const MPduItem* l = left;
    const MPduItem* r = right;
    if (l->priority < r->priority) return -1;
    if (l->priority > r->priority) return 1;
    return 0;
}


static void pdunet_visit_map_pdu(
    PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    if (pdu == NULL || pdu->pdu == NULL) return;
    PduObject* c_pdu = data;
    if (c_pdu == NULL || c_pdu->pdu == NULL) return;
    if (pdu->pdu->dir != c_pdu->pdu->dir) return;

    if (pdu->pdu->container.id == c_pdu->pdu->id) {
        vector_push(&c_pdu->container.pdu_list,
            &(MPduItem){
                .id = pdu->pdu->id,
                .priority = pdu->pdu->container.priority,
                .pdu = pdu,
            });
        log_trace(
            "Container: [%u] L-PDU[%u] <-map- [%u] I-PDU[%u], priority=%u",
            c_pdu->matrix.pdu_idx, c_pdu->pdu->id, pdu->matrix.pdu_idx,
            pdu->pdu->id, pdu->pdu->container.priority);
    }
}


void pdunet_visit_setup_containers(
    PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    UNUSED(data);
    if (pdu == NULL || pdu->pdu == NULL) return;
    if (pdu->container.header == HeaderFormatNone) return;

    /* Setup the Container mapping. */
    pdu->container.pdu_list = vector_make(sizeof(MPduItem), 4, _sort_mpdu);
    pdunet_visit(net, NULL, pdunet_visit_map_pdu, pdu);
    vector_sort(&pdu->container.pdu_list);
}

static size_t header_length[] = {
    [HeaderFormatNone] = 0,
    [HeaderFormatStatic] = 0,
    [HeaderFormatShort] = 4,
    [HeaderFormatFull] = 8,
};

void pdunet_visit_container_mapto(
    PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(data);
    assert(net);
    if (pdu == NULL || pdu->pdu == NULL) return;
    if (pdu->pdu->dir != PduDirectionTx) return;
    if (pdu->container.header == HeaderFormatNone) return;

    // L-PDU.
    uint8_t* payload = NULL;
    vector_at(&(net->matrix.payload), pdu->matrix.pdu_idx, &payload);
    assert(payload);
    size_t payload_len = pdu->pdu->length;
    size_t payload_offset = 0;

    // I-PDUs (sorted by container.priority).
    size_t count = vector_len(&pdu->container.pdu_list);
    for (size_t i = 0; i < count; i++) {
        MPduItem* pi = vector_at(&pdu->container.pdu_list, i, NULL);
        assert(pi);
        assert(pi->pdu);
        if (pi->pdu->needs_tx != true) continue;
        size_t len = pi->pdu->pdu->length;
        if ((len + header_length[pdu->container.header]) >
            (payload_len - payload_offset)) {
            // No room for this PDU, but others might still fit, so continue.
            pi->pdu->needs_tx = false;
            pi->pdu->checksum = 0;  // Triggers update of needs_tx on next.
            continue;
        }

        // Map in this I-PDU.
        log_trace("Map to Container: [%u] L-PDU[%u] <-map- [%u] I-PDU[%u], "
                  "offset=%u, len=%u/%u",
            pdu->matrix.pdu_idx, pdu->pdu->id, pi->pdu->pdu->id,
            pi->pdu->matrix.pdu_idx, payload_offset, len,
            len + header_length[pdu->container.header]);
        uint8_t* pi_payload = NULL;
        vector_at(&(net->matrix.payload), pi->pdu->matrix.pdu_idx, &pi_payload);
        assert(pi_payload);
        switch (pdu->container.header) {
        case HeaderFormatStatic:
            break;
        case HeaderFormatShort:
            len = (uint8_t)len;
            payload[payload_offset + 0] = (uint8_t)(pi->id >> 16);
            payload[payload_offset + 1] = (uint8_t)(pi->id >> 8);
            payload[payload_offset + 2] = (uint8_t)pi->id;
            payload[payload_offset + 3] = (uint8_t)len;
            break;
        case HeaderFormatFull:
            len = (uint32_t)len;
            payload[payload_offset + 0] = (uint8_t)(pi->id >> 24);
            payload[payload_offset + 1] = (uint8_t)(pi->id >> 16);
            payload[payload_offset + 2] = (uint8_t)(pi->id >> 8);
            payload[payload_offset + 3] = (uint8_t)pi->id;
            payload[payload_offset + 4] = (uint8_t)(len >> 24);
            payload[payload_offset + 5] = (uint8_t)(len >> 16);
            payload[payload_offset + 6] = (uint8_t)(len >> 8);
            payload[payload_offset + 7] = (uint8_t)len;
            break;
        default:
            break;
        }
        payload_offset += header_length[pdu->container.header];
        memcpy(payload + payload_offset, pi_payload, len);
        payload_offset += len;
        pdu->needs_tx = true;
        pi->pdu->needs_tx = false;
    }
    pdu->checksum = pdunet_checksum(payload, payload_len);
}

void pdunet_visit_container_mapfrom(
    PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    UNUSED(pdu);
    UNUSED(data);
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
