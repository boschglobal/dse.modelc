// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <math.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/model/pdunet/network.h>
#include <dse/modelc/pdunet.h>
#include <dse/mocks/simmock.h>


extern uint8_t __log_level__;

typedef struct ModelCMock {
    SimulationSpec     sim;
    ModelInstanceSpec* mi;
    PduNetworkDesc*    net;
} ModelCMock;


static int test_setup(void** state)
{
    ModelCMock* mock = calloc(1, sizeof(ModelCMock));
    assert_non_null(mock);

    int             rc;
    ModelCArguments args;
    char*           argv[] = {
        (char*)"test_pdunet",
        (char*)"--name=flexray",
        (char*)"resources/model/pdunet.yaml",
    };

    modelc_set_default_args(&args, "test", 0.005, 0.005);
    args.log_level = __log_level__;
    modelc_parse_arguments(&args, ARRAY_SIZE(argv), argv, "PDU-Network");
    rc = modelc_configure(&args, &mock->sim);
    assert_int_equal(rc, 0);
    ModelInstanceSpec* mi = modelc_get_model_instance(&mock->sim, args.name);
    assert_non_null(mi);
    mock->mi = mi;

    /* Return the mock. */
    *state = mock;
    return 0;
}


static int test_setup_lua(void** state)
{
    ModelCMock* mock = calloc(1, sizeof(ModelCMock));
    assert_non_null(mock);

    int             rc;
    ModelCArguments args;
    char*           argv[] = {
        (char*)"test_pdunet",
        (char*)"--name=flexray",
        (char*)"resources/model/pdunet_lua.yaml",
    };

    modelc_set_default_args(&args, "test", 0.005, 0.005);
    args.log_level = __log_level__;
    modelc_parse_arguments(&args, ARRAY_SIZE(argv), argv, "PDU-Network");
    rc = modelc_configure(&args, &mock->sim);
    assert_int_equal(rc, 0);
    ModelInstanceSpec* mi = modelc_get_model_instance(&mock->sim, args.name);
    assert_non_null(mi);
    mock->mi = mi;

    /* Return the mock. */
    *state = mock;
    return 0;
}


static int test_teardown(void** state)
{
    ModelCMock* mock = *state;

    if (mock) {
        if (mock->net) {
            pdunet_destroy(mock->net);
        }
        if (mock->mi) {
            dse_yaml_destroy_doc_list(mock->mi->yaml_doc_list);
        }
        modelc_exit(&mock->sim);
        free(mock);
    }

    return 0;
}


void test_pdu_flexray_network_parse(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    assert_non_null(net->mi);
    mock->net = net;  // Teardown will destroy.
    assert_non_null(net);
    assert_non_null(net->mi);
    assert_ptr_equal(net->ncodec, ncodec);
    assert_ptr_equal(net->mi, mock->mi);
    assert_int_equal(vector_len(&net->pdus), 0);
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    // __log_level__ = LOG_TRACE;
    rc = pdunet_parse(net, labels);
    // __log_level__ = LOG_QUIET;
    assert_int_equal(rc, 0);
    assert_non_null(net->doc);
    assert_int_equal(vector_len(&net->pdus), 4);
    // Schedule.
    assert_double_equal(net->schedule.simulation_time, 0.0, 0);
    assert_double_equal(net->schedule.step_size, 0.0005, 0);
    assert_double_equal(net->schedule.epoch_offset, 10.0, 0);
    // Metadata Flexray
    assert_int_equal(
        NCodecPduTransportTypeFlexray, net->network.transport_type);
    assert_non_null(net->network.metadata.config);
    NCodecPduFlexrayConfig* metadata = net->network.metadata.config;
    assert_int_equal(metadata->inhibit_null_frames, true);
    // assert_int_equal(metadata->macrotick_per_cycle,8000 );
    assert_int_equal(metadata->microtick_per_cycle, 80);
    assert_int_equal(metadata->network_idle_start, 10);
    assert_int_equal(metadata->static_slot_length, 64);
    assert_int_equal(metadata->static_slot_count, 48);
    assert_int_equal(metadata->minislot_length, 10);
    assert_int_equal(metadata->minislot_count, 300);
    assert_int_equal(metadata->static_slot_payload_length, 32);
    assert_int_equal(metadata->bit_rate, NCodecPduFlexrayBitrate10);
    assert_int_equal(metadata->channel_enable, NCodecPduFlexrayChannelAB);
    assert_int_equal(metadata->wakeup_channel_select, NCodecPduFlexrayChannelA);
    assert_int_equal(metadata->single_slot_enabled, true);
    // PDU
    PduItem pdu;
    vector_at(&net->pdus, 0, &pdu);
    assert_int_equal(pdu.id, 101);
    assert_int_equal(pdu.length, 64);
    assert_int_equal(pdu.dir, 2);
    assert_non_null(pdu.name);
    assert_string_equal(pdu.name, "ONE");
    assert_non_null(pdu.lua.encode);
    // Schedule.
    assert_double_equal(pdu.schedule.phase, -0.001, 0);
    assert_double_equal(pdu.schedule.interval, 0.005, 0);
    // Functions.
    assert_string_equal(
        pdu.lua.encode, "function FR__ONE-encode(ctx) return ctx end\n");
    assert_non_null(pdu.lua.decode);
    assert_string_equal(
        pdu.lua.decode, "function FR__ONE-decode(ctx) return ctx end\n");
    // PDU Metadata Flexray
    assert_non_null(net->network.metadata.config);
    NCodecPduFlexrayLpduConfig* pdu_metadata = pdu.metadata.config;
    assert_int_equal(pdu_metadata->slot_id, 11);
    assert_int_equal(pdu_metadata->payload_length, 64);
    assert_int_equal(pdu_metadata->cycle_repetition, 8);
    assert_int_equal(pdu_metadata->base_cycle, 4);
    assert_int_equal(pdu_metadata->direction, 2);
    assert_int_equal(pdu_metadata->channel, 1);
    assert_int_equal(pdu_metadata->transmit_mode, 1);
    assert_int_equal(pdu_metadata->inhibit_null, true);
    // PDU Signals
    assert_int_equal(vector_len(&pdu.signals), 2);
    PduSignalItem signal;
    vector_at(&pdu.signals, 0, &signal);
    assert_non_null(signal.name);
    assert_string_equal(signal.name, "SIG_A");
    assert_int_equal(signal.start_bit, 4);
    assert_int_equal(signal.length_bits, 12);
    assert_double_equal(signal.factor, 0.1, 0);
    assert_double_equal(signal.offset, -40, 0);
    assert_double_equal(signal.min, -30, 0);
    assert_double_equal(signal.max, 215, 0);
    assert_string_equal(signal.lua.encode,
        "function FR__ONE__SIG_A-encode(ctx) return ctx end\n");
    assert_non_null(signal.lua.decode);
    assert_string_equal(signal.lua.decode,
        "function FR__ONE__SIG_A-decode(ctx) return ctx end\n");
}


typedef struct matrix_check {
    size_t      idx;  // Index to matrix.signal
    const char* name;
    size_t      pdu_idx;     // index to matrix.pdu
    size_t      signal_idx;  // Index matrix.pdu.signal
    double      phys;
    uint64_t    raw;
    double      factor;
    double      offset;
    double      min;
    double      max;
    int         encode_ref;
    int         decode_ref;
    uint16_t    start_bit;
    uint16_t    length_bits;
    struct {
        double   phys;
        uint64_t raw;
    } expect;
} matrix_check;


typedef struct matrix_pdu_check {
    size_t   idx;  // Index to matrix.pdu
    uint32_t phase;
    uint32_t interval;
} matrix_pdu_check;

void test_pdunet_transform(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);

    pdunet_transform(net, NULL);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);
    assert_int_equal(vector_len(&net->matrix.payload), 4);
    assert_int_equal(vector_len(&net->matrix.range), 2);
    assert_int_equal(vector_len(&net->matrix.signal.pdu_idx), 8);
    assert_int_equal(vector_len(&net->matrix.signal.signal_idx), 8);
    assert_int_equal(vector_len(&net->matrix.signal.name), 8);
    assert_int_equal(vector_len(&net->matrix.signal.skip), 8);
    assert_int_equal(vector_len(&net->matrix.signal.phys), 8);
    assert_int_equal(vector_len(&net->matrix.signal.raw), 8);
    assert_int_equal(vector_len(&net->matrix.signal.factor), 8);
    assert_int_equal(vector_len(&net->matrix.signal.offset), 8);
    assert_int_equal(vector_len(&net->matrix.signal.min), 8);
    assert_int_equal(vector_len(&net->matrix.signal.max), 8);
    assert_int_equal(vector_len(&net->matrix.signal.encode), 8);
    assert_int_equal(vector_len(&net->matrix.signal.decode), 8);
    assert_int_equal(vector_len(&net->matrix.signal.start_bit), 8);
    assert_int_equal(vector_len(&net->matrix.signal.length_bits), 8);
    assert_int_equal(net->matrix.signal.count, 8);

    matrix_pdu_check pdu_checks[] = {
        // clang-format off
        { .idx = 0 }, // NOLINT
        { .idx = 1 }, // NOLINT
        { .idx = 2, .phase = 8, .interval = 10 }, // NOLINT
        { .idx = 3, .phase = 18, .interval = 20  }, // NOLINT
        // clang-format on
    };
    for (size_t i = 0; i < ARRAY_SIZE(pdu_checks); i++) {
        matrix_pdu_check c = pdu_checks[i];
        log_info("PDU Check[%u] idx=%u", i, c.idx);
    }

    matrix_check checks[] = {
        { 0, "SIG_C", 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 4 },
        { 1, "SIG_D", 0, 1, 0, 0, 1, -50, 0, 200, 0, 0, 10, 8 },
        { 2, "SIG_G", 1, 0, 0.0, 0, 1.0, 0.0, NAN, NAN, 0, 0, 0, 4 },
        { 3, "SIG_H", 1, 1, 0, 0, 2.0, -100, -80, 300, 0, 0, 8, 8 },
        { 4, "SIG_A", 2, 0, 0, 0, 0.1, -40, -30, 215, 0, 0, 4, 12 },
        { 5, "SIG_B", 2, 1, 0, 0, 1, -50, -50, 50, 0, 0, 13, 20 },
        { 6, "SIG_E", 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 4 },
        { 7, "SIG_F", 3, 1, 0, 0, 1, -50, 0, 200, 0, 0, 10, 8 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(checks); i++) {
        matrix_check c = checks[i];
        log_info("Check[%u] name=%s, pdu_idx=%u, signal_idx=%u", i, c.name,
            c.pdu_idx, c.signal_idx);

        // clang-format off
        assert_int_equal(*(size_t*)vector_at(&net->matrix.signal.pdu_idx, c.idx, NULL), c.pdu_idx); // NOLINT
        assert_int_equal(*(size_t*)vector_at(&net->matrix.signal.signal_idx, c.idx, NULL), c.signal_idx); // NOLINT
        assert_string_equal(*(char**)vector_at(&net->matrix.signal.name, c.idx, NULL), c.name); // NOLINT
        assert_double_equal(*(double*)vector_at(&net->matrix.signal.phys, c.idx, NULL), c.phys, 0); // NOLINT
        assert_int_equal(*(uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL), c.raw); // NOLINT
        assert_double_equal(*(double*)vector_at(&net->matrix.signal.factor,c.idx, NULL), c.factor, 0); // NOLINT
        assert_double_equal(*(double*)vector_at(&net->matrix.signal.offset, c.idx, NULL), c.offset, 0); // NOLINT
        assert_double_equal(*(double*)vector_at(&net->matrix.signal.min, c.idx, NULL), c.min, 0); // NOLINT
        assert_double_equal(*(double*)vector_at(&net->matrix.signal.max,c.idx, NULL), c.max,0); // NOLINT
        assert_int_equal(*(int*)vector_at(&net->matrix.signal.encode, c.idx, NULL), c.encode_ref); // NOLINT
        assert_int_equal(*(int*)vector_at(&net->matrix.signal.decode, c.idx, NULL), c.decode_ref); // NOLINT
        assert_int_equal(*(uint16_t*)vector_at(&net->matrix.signal.start_bit, c.idx, NULL), c.start_bit); // NOLINT
        assert_int_equal(*(uint16_t*)vector_at(&net->matrix.signal.length_bits, c.idx, NULL), c.length_bits); // NOLINT
        // clang-format on
        size_t pdu_idx =
            *(size_t*)vector_at(&net->matrix.signal.pdu_idx, c.idx, NULL);
        size_t signal_idx =
            *(size_t*)vector_at(&net->matrix.signal.signal_idx, c.idx, NULL);
        PduObject*     o = vector_at(&net->matrix.pdu, pdu_idx, NULL);
        PduItem*       p = o->pdu;
        PduSignalItem* s = vector_at(&p->signals, signal_idx, NULL);
        assert_string_equal(s->name, c.name);
    }

    Vector r1 = vector_make(sizeof(size_t), 4, NULL);
    vector_push(&r1, &(size_t){ 0 });
    vector_push(&r1, &(size_t){ 1 });
    Vector r2 = vector_make(sizeof(size_t), 4, NULL);
    vector_push(&r2, &(size_t){ 2 });
    vector_push(&r2, &(size_t){ 3 });
    PduRange range_checks[] = {
        { .dir = PduDirectionRx, .offset = 0, .length = 4, .pdu_list = r1 },
        { .dir = PduDirectionTx, .offset = 4, .length = 4, .pdu_list = r2 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(range_checks); i++) {
        PduRange c = range_checks[i];
        log_info("Range Check[%u] dir=%u, offset=%u, length=%u", i, c.dir,
            c.offset, c.length);

        PduRange* r = vector_at(&net->matrix.range, i, NULL);
        assert_non_null(r);

        // clang-format off
        assert_int_equal(r->dir, c.dir);
        assert_int_equal(r->offset, c.offset);
        assert_int_equal(r->length, c.length);
        assert_int_equal(vector_len(&r->pdu_list), vector_len(&c.pdu_list));
        for (size_t j = 0; j < vector_len(&r->pdu_list); j++) {
            size_t r_pdu_idx = 42;
            size_t c_pdu_idx = 42;
            vector_at(&r->pdu_list, j, &r_pdu_idx);
            vector_at(&c.pdu_list, j, &c_pdu_idx);
            assert_int_equal(r_pdu_idx, c_pdu_idx);
        }
        // clang-format on
    }
    vector_reset(&r1);
    vector_reset(&r2);
}


typedef struct pdu_check {
    size_t                 idx;  // Index to matrix.pdu
    const char*            name;
    uint32_t               pdu_id;
    size_t                 payload_length;
    NCodecPduTransportType transport;

    uint16_t                   frame_config_index;
    NCodecPduFlexrayLpduStatus status;
    uint32_t                   checksum;
    bool                       needs_tx;
} pdu_check;


void test_pdunet_ncodec_pdu(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);
    pdunet_transform(net, NULL);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);
    assert_int_equal(vector_len(&net->matrix.payload), 4);

    pdu_check checks[] = {
        // clang-format off
        { .idx = 0, .name = "TWO", .pdu_id = 202, .payload_length = 64, .transport = NCodecPduTransportTypeFlexray }, // NOLINT
        { .idx = 1, .name = "FOUR", .pdu_id = 404, .payload_length = 64, .transport = NCodecPduTransportTypeFlexray }, // NOLINT
        { .idx = 2, .name = "ONE", .pdu_id = 101, .payload_length = 64, .transport = NCodecPduTransportTypeFlexray }, // NOLINT
        { .idx = 3, .name = "THREE", .pdu_id = 303, .payload_length = 64, .transport = NCodecPduTransportTypeFlexray }, // NOLINT
        // clang-format on
    };
    for (size_t i = 0; i < ARRAY_SIZE(checks); i++) {
        pdu_check c = checks[i];
        log_info("PDU Check[%u] idx=%u, name=%s, id=%u, length=%u", i, c.idx,
            c.name, c.pdu_id, c.payload_length);

        PduObject* o = vector_at(&net->matrix.pdu, c.idx, NULL);
        assert_string_equal(o->pdu->name, c.name);
        assert_int_equal(o->pdu->id, c.pdu_id);
        assert_int_equal(o->pdu->length, c.payload_length);

        uint8_t** payload = vector_at(&net->matrix.payload, c.idx, NULL);
        assert_non_null(*payload);

        assert_int_equal(o->ncodec.pdu.id, c.pdu_id);
        assert_int_equal(o->ncodec.pdu.payload_len, c.payload_length);
        assert_ptr_equal(o->ncodec.pdu.payload, *payload);
    }
}


void test_pdunet_linear_part_tx(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);
    pdunet_transform(net, NULL);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);

    // TX Direction: Phys -> Raw
    matrix_check checks[] = {
        { .idx = 4, .name = "SIG_A", .phys = 10, .raw = 500 },
        { .idx = 4, .name = "SIG_A", .phys = 11, .raw = 510 },
        { .idx = 0, .name = "SIG_C", .phys = 4, .raw = 0 },  // Rx, no change.
        { .idx = 5, .name = "SIG_B", .phys = 49, .raw = 99 },
        { .idx = 5,
            .name = "SIG_B",
            .phys = 51,
            .raw = 99 },  // Range max, no change.
        { .idx = 5, .name = "SIG_B", .phys = 50, .raw = 100 },
        { .idx = 5, .name = "SIG_B", .phys = -49, .raw = 1 },
        { .idx = 5,
            .name = "SIG_B",
            .phys = -51,
            .raw = 1 },  // Range min, no change.
        { .idx = 5, .name = "SIG_B", .phys = -50, .raw = 0 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(checks); i++) {
        matrix_check c = checks[i];
        log_info("Check TX[%u] idx=%u, name=%s, phys=%f, raw=%u (expect)", i,
            c.idx, c.name, c.phys, c.raw);

        // Check the name is as expected.
        size_t pdu_idx =
            *(size_t*)vector_at(&net->matrix.signal.pdu_idx, c.idx, NULL);
        size_t signal_idx =
            *(size_t*)vector_at(&net->matrix.signal.signal_idx, c.idx, NULL);
        PduObject*     o = vector_at(&net->matrix.pdu, pdu_idx, NULL);
        PduSignalItem* s = vector_at(&o->pdu->signals, signal_idx, NULL);
        assert_string_equal(s->name, c.name);

        double* phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        assert_non_null(phys);
        assert_non_null(raw);
        *phys = c.phys;
        pdunet_encode_linear(net, NULL);
        assert_double_equal(*phys, c.phys, 0);
        assert_int_equal(*raw, c.raw);
    }
}


void test_pdunet_linear_part_rx(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);
    pdunet_transform(net, NULL);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);

    //__log_level__ = LOG_TRACE;
    // RX Direction: Raw -> Phys
    matrix_check checks[] = {
        { .idx = 3, .name = "SIG_H", .phys = 100, .raw = 100 },
        { .idx = 3, .name = "SIG_H", .phys = 0, .raw = 50 },
        { .idx = 3, .name = "SIG_H", .phys = 300, .raw = 200 },  // Clamp on max
        { .idx = 3, .name = "SIG_H", .phys = 298, .raw = 199 },  // Clamp on max
        { .idx = 3,
            .name = "SIG_H",
            .phys = 298,
            .raw = 201 },  // Clamp on max, no change.
        { .idx = 3, .name = "SIG_H", .phys = -80, .raw = 10 },  // Clamp on min
        { .idx = 3, .name = "SIG_H", .phys = -78, .raw = 11 },  // Clamp on min
        { .idx = 3,
            .name = "SIG_H",
            .phys = -78,
            .raw = 8 },  // Clamp on min, no change
        { .idx = 4, .name = "SIG_A", .phys = 0, .raw = 500 },  // Tx, no change
    };
    for (size_t i = 0; i < ARRAY_SIZE(checks); i++) {
        matrix_check c = checks[i];
        log_info("Check RX[%u] idx=%u, name=%s, phys=%f, raw=%u (expect)", i,
            c.idx, c.name, c.phys, c.raw);

        // Check the name is as expected.
        size_t pdu_idx =
            *(size_t*)vector_at(&net->matrix.signal.pdu_idx, c.idx, NULL);
        size_t signal_idx =
            *(size_t*)vector_at(&net->matrix.signal.signal_idx, c.idx, NULL);
        PduObject*     o = vector_at(&net->matrix.pdu, pdu_idx, NULL);
        PduSignalItem* s = vector_at(&o->pdu->signals, signal_idx, NULL);
        assert_string_equal(s->name, c.name);

        double* phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        assert_non_null(phys);
        assert_non_null(raw);
        *raw = c.raw;
        pdunet_decode_linear(net, NULL);
        assert_double_equal(*phys, c.phys, 0);
        assert_int_equal(*raw, c.raw);
    }
}


void test_pdunet_pack_tx(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);
    pdunet_transform(net, NULL);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);

    // Set some signal values.
    matrix_check values[] = {
        { .idx = 6, .name = "SIG_E", .phys = 6, .raw = 6 },
        { .idx = 7, .name = "SIG_F", .phys = 50, .raw = 100 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        matrix_check c = values[i];
        log_info("Check TX[%u] idx=%u, name=%s, phys=%f, raw=%u (expect)", i,
            c.idx, c.name, c.phys, c.raw);

        // Check the name is as expected.
        size_t pdu_idx =
            *(size_t*)vector_at(&net->matrix.signal.pdu_idx, c.idx, NULL);
        size_t signal_idx =
            *(size_t*)vector_at(&net->matrix.signal.signal_idx, c.idx, NULL);
        PduObject*     o = vector_at(&net->matrix.pdu, pdu_idx, NULL);
        PduSignalItem* s = vector_at(&o->pdu->signals, signal_idx, NULL);
        assert_string_equal(s->name, c.name);

        double* phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        *phys = c.phys;
        pdunet_encode_linear(net, NULL);
        assert_int_equal(*raw, c.raw);
    }

    // Pack the raw values and check the payload.
    pdunet_encode_pack(net, NULL);
    PduObject* o = vector_at(&net->matrix.pdu, 3, NULL);
    uint8_t    payload[64] = {
        [0] = 0x18,  // 6
        [1] = 0x90,  // 100/x64, lower part
        [2] = 0x01,  // 100/x64, upper part
    };
    assert_memory_equal(o->ncodec.pdu.payload, payload, 64);
}


void test_pdunet_unpack_rx(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);
    pdunet_transform(net, NULL);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);

    // __log_level__ = LOG_TRACE;
    // Pack the raw values and check the payload.
    uint8_t** _ = vector_at(&net->matrix.payload, 0, NULL);
    uint8_t*  payload = *_;
    payload[0] = 0x18;  // 6
    payload[1] = 0x90;  // 100/x64, lower part
    payload[2] = 0x01;  // 100/x64, upper part
    pdunet_decode_unpack(net, NULL);

    // Check some signal values.
    matrix_check values[] = {
        { .idx = 0, .name = "SIG_C", .phys = 6, .raw = 6 },
        { .idx = 1, .name = "SIG_D", .phys = 50, .raw = 100 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        matrix_check c = values[i];
        log_info("Check RX[%u] idx=%u, name=%s, phys=%f, raw=%u (expect)", i,
            c.idx, c.name, c.phys, c.raw);

        // Check the name is as expected.
        size_t pdu_idx =
            *(size_t*)vector_at(&net->matrix.signal.pdu_idx, c.idx, NULL);
        size_t signal_idx =
            *(size_t*)vector_at(&net->matrix.signal.signal_idx, c.idx, NULL);
        PduObject*     o = vector_at(&net->matrix.pdu, pdu_idx, NULL);
        PduSignalItem* s = vector_at(&o->pdu->signals, signal_idx, NULL);
        assert_string_equal(s->name, c.name);

        double* phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        pdunet_decode_linear(net, NULL);
        assert_int_equal(*raw, c.raw);
        assert_double_equal(*phys, c.phys, 0);
    }
}


void test_pdunet_config(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);
    rc = pdunet_configure(net);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);

    // Check Lua functions were parsed.
    assert_non_null(net->lua.global);
    PduItem pdu;
    vector_at(&net->pdus, 0, &pdu);
    assert_non_null(pdu.lua.decode);
    assert_non_null(pdu.lua.encode);
    PduSignalItem signal;
    vector_at(&pdu.signals, 0, &signal);
    assert_non_null(signal.lua.decode);
    assert_non_null(signal.lua.encode);

    // Check Lua functions are loaded (ref != 0).
    vector_at(&net->pdus, 0, &pdu);
    assert_int_not_equal(pdu.lua.encode_ref, 0);
    assert_int_equal(pdu.lua.encode_ref > 0, true);
    assert_int_equal(pdu.lua.decode_ref, 0);
    vector_at(&pdu.signals, 1, &signal);
    assert_int_not_equal(signal.lua.encode_ref, 0);
    assert_int_equal(signal.lua.encode_ref > 0, true);
    assert_int_equal(signal.lua.decode_ref, 0);
}


void test_pdunet_lua_tx(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    // __log_level__ = LOG_TRACE;
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);

    rc = pdunet_configure(net);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);

    rc = pdunet_transform(net, NULL);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);

    matrix_check signal_checks[] = {
        // clang-format off
        { .idx = 4, .pdu_idx = 2, .signal_idx = 1, .name = "Alive",
          .phys = 5, .raw = 5, .encode_ref = 1, .decode_ref = 0, .expect = {.phys = 6, .raw = 6}}, // NOLINT
        { .idx = 5, .pdu_idx = 2, .signal_idx = 2, .name = "FOO",
          .phys = 0, .raw = 0, .encode_ref = 1, .decode_ref = 0, .expect = {.phys = 42, .raw = 24}}, // NOLINT
        { .idx = 6, .pdu_idx = 2, .signal_idx = 3, .name = "BAR",
          .phys = 0, .raw = 0, .encode_ref = 1, .decode_ref = 0, .expect = {.phys = 0, .raw = 42}}, // NOLINT
        { .idx = 7, .pdu_idx = 2, .signal_idx = 4, .name = "FUBAR",
          .phys = 7, .raw = 9, .encode_ref = 1, .decode_ref = 0, .expect = {.phys = 7, .raw = 9}}, // NOLINT
        // clang-format on
    };
    for (size_t i = 0; i < ARRAY_SIZE(signal_checks); i++) {
        matrix_check c = signal_checks[i];
        log_info("Check[%u] name=%s, idx=%u, pdu_idx=%u, signal_idx=%u", i,
            c.name, c.idx, c.pdu_idx, c.signal_idx);
        // Check the name is as expected.
        PduObject*     o = vector_at(&net->matrix.pdu, c.pdu_idx, NULL);
        PduSignalItem* s = vector_at(&o->pdu->signals, c.signal_idx, NULL);
        assert_string_equal(s->name, c.name);
        // Check lua functions are loaded.
        assert_int_equal(
            *(int*)vector_at(&net->matrix.signal.encode, c.idx, NULL) > 0,
            c.encode_ref != 0);
        assert_int_equal(
            *(int*)vector_at(&net->matrix.signal.decode, c.idx, NULL) > 0,
            c.decode_ref != 0);
        // Set signal values.
        double* phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        assert_non_null(phys);
        assert_non_null(raw);
        *phys = c.phys;
        *raw = c.raw;
    }
    pdunet_encode_linear(net, NULL);
    for (size_t i = 0; i < ARRAY_SIZE(signal_checks); i++) {
        matrix_check c = signal_checks[i];
        double*      phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        assert_non_null(phys);
        assert_non_null(raw);
        assert_double_equal(*phys, c.expect.phys, 0);
        assert_int_equal(*raw, c.expect.raw);
    }

    // Pack the raw values and check the payload.
    pdunet_encode_pack(net, NULL);
    PduObject* o = vector_at(&net->matrix.pdu, 2, NULL);
    uint8_t    payload[64] = {
        [0] = 0x51,  // checksum
        [1] = 0x06,  // 6
        [2] = 0x18,  // 24
        [3] = 0x2a,  // 42
        [4] = 0x09,  // 9
    };
    assert_memory_equal(o->ncodec.pdu.payload, payload, 64);
}


void test_pdunet_lua_rx(void** state)
{
    ModelCMock* mock = *state;
    int         rc = 0;
    assert_non_null(mock->mi);

    void*           ncodec = NULL;
    PduNetworkDesc* net =
        pdunet_create(mock->mi, ncodec, NULL, NULL, NULL, NULL);
    mock->net = net;  // Teardown will destroy.
    SchemaLabel labels[] = {
        { .name = "name", .value = "FlexRay" },
        { .name = "model", .value = "flexray" },
        {},
    };
    // __log_level__ = LOG_TRACE;
    rc = pdunet_parse(net, labels);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);

    rc = pdunet_configure(net);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->pdus), 4);

    rc = pdunet_transform(net, NULL);
    assert_int_equal(rc, 0);
    assert_int_equal(vector_len(&net->matrix.pdu), 4);

    // Pack the raw values and check the payload.
    uint8_t** _ = vector_at(&net->matrix.payload, 0, NULL);
    uint8_t*  payload = *_;
    payload[0] = 0x51;  // checksum
    payload[1] = 0x06;  // 6
    payload[2] = 0x18;  // 24
    payload[3] = 0x2a;  // 42
    payload[4] = 0x09;  // 9
    pdunet_decode_unpack(net, NULL);

    // Check the payload (some fields modified).
    uint8_t check_payload[64] = {
        [0] = 0x2b,  // checksum, modified if passed.
        [1] = 0x06,  // 6
        [2] = 0x18,  // 24
        [3] = 0x2a,  // 42
        [4] = 0x09,  // 9
    };
    PduObject* o = vector_at(&net->matrix.pdu, 0, NULL);
    assert_memory_equal(o->ncodec.pdu.payload, check_payload, 64);

    matrix_check signal_checks[] = {
        // clang-format off
        { .idx = 0, .pdu_idx = 0, .signal_idx = 0, .name = "FOO",
          .phys = 12, .raw = 24, .encode_ref = 0, .decode_ref = 1, .expect = {.phys = 42, .raw = 24}}, // NOLINT
        { .idx = 1, .pdu_idx = 0, .signal_idx = 1, .name = "BAR",
          .phys = 6, .raw = 8, .encode_ref = 0, .decode_ref = 1, .expect = {.phys = 68, .raw = 44}}, // NOLINT
        // clang-format on
    };
    pdunet_decode_linear(net, NULL);
    for (size_t i = 0; i < ARRAY_SIZE(signal_checks); i++) {
        matrix_check c = signal_checks[i];
        log_info("Check[%u] name=%s, idx=%u, pdu_idx=%u, signal_idx=%u", i,
            c.name, c.idx, c.pdu_idx, c.signal_idx);
        // Check the name is as expected.
        PduObject*     o = vector_at(&net->matrix.pdu, c.pdu_idx, NULL);
        PduSignalItem* s = vector_at(&o->pdu->signals, c.signal_idx, NULL);
        assert_string_equal(s->name, c.name);
        // Check lua functions are loaded.
        assert_int_equal(
            *(int*)vector_at(&net->matrix.signal.encode, c.idx, NULL) > 0,
            c.encode_ref != 0);
        assert_int_equal(
            *(int*)vector_at(&net->matrix.signal.decode, c.idx, NULL) > 0,
            c.decode_ref != 0);
        // Set signal values.
        double* phys =
            (double*)vector_at(&net->matrix.signal.phys, c.idx, NULL);
        uint64_t* raw =
            (uint64_t*)vector_at(&net->matrix.signal.raw, c.idx, NULL);
        assert_non_null(phys);
        assert_non_null(raw);
        assert_double_equal(*phys, c.expect.phys, 0);
        assert_int_equal(*raw, c.expect.raw);
    }
}


int run_model_pdu_tests(void)
{
    void* s = test_setup;
    void* sl = test_setup_lua;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_pdu_flexray_network_parse, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_transform, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_ncodec_pdu, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_linear_part_tx, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_linear_part_rx, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_pack_tx, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_unpack_rx, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_config, sl, t),
        cmocka_unit_test_setup_teardown(test_pdunet_lua_tx, sl, t),
        cmocka_unit_test_setup_teardown(test_pdunet_lua_rx, sl, t),
    };

    return cmocka_run_group_tests_name("PDU Network", tests, NULL, NULL);
}
