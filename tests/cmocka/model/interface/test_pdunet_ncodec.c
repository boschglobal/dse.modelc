// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/mocks/simmock.h>
#include <dse/modelc/model.h>
#include <dse/modelc/pdunet.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>
#include <dse/ncodec/codec/ab/codec.h>


#define REPO_PATH     "../../../../"
#define EXAMPLES_PATH REPO_PATH "dse/modelc/build/_out/examples/"
#define RESOURCES_REL_PATH                                                     \
    "../../../../../../tests/cmocka/build/_out/resources/model/"


static char __entry_path__[PATH_MAX];

typedef struct TestData {
    SimMock*        mock;
    ABCodecBusModel bm_save;
    NCODEC*         nc;
} TestData;

static int test_setup(void** state)
{
    TestData* data = calloc(1, sizeof(TestData));
    *state = data;
    return 0;
}

static int test_teardown(void** state)
{
    TestData* data = *state;

    if (data->nc) {
        ((ABCodecInstance*)data->nc)->reader.bus_model = data->bm_save;
    }
    if (data->mock) {
        simmock_exit(data->mock, true);
        simmock_free(data->mock);
    }
    free(data);

    chdir(__entry_path__);
    return 0;
}


#define PDUNET_INST_NAME  "pdunet_inst"
#define PDUNET_MODEL_PATH EXAMPLES_PATH "/pdunet/frnet"

static const char* inst_names[] = {
    PDUNET_INST_NAME,
};
static char* inst_argv[] = {
    (char*)"test_pdunet",
    (char*)"--name=" PDUNET_INST_NAME,
    (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
    (char*)"data/model.yaml",
    (char*)"data/network.yaml",
    (char*)"data/simulation.yaml",
};

/*
When a model is created, for each NCodec object, an associated PDU Network
will be created (if a matching network configuration is found). The PDU Network
is available but must be manually operated.
*/
void test_pdunet_setup(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);
    simmock_setup(mock, "scalar", "network");
    assert_non_null(model->sv_network);
    assert_non_null(model->sv_signal);
}


void test_pdunet_find(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);
    simmock_setup(mock, "scalar", "network");

    PduNetworkDesc* net = NULL;
    assert_non_null(model->mi);
    assert_non_null(model->sv_network->ncodec[0]);
    net = pdunet_find(model->mi, model->sv_network->ncodec[0]);
    assert_non_null(net);
    assert_ptr_equal(net->ncodec, model->sv_network->ncodec[0]);
    assert_string_equal(net->name, "FR_A");
}


void _visit_func(PduNetworkDesc* net, PduObject* pdu, void* data)
{
    assert_non_null(net);
    vector_push((Vector*)data, (void*)pdu->pdu->name);
}

void test_pdunet_visit(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);
    simmock_setup(mock, "scalar", "network");
    PduNetworkDesc* net = pdunet_find(model->mi, model->sv_network->ncodec[0]);
    assert_non_null(net);

    Vector visit_list = vector_make(sizeof(char*), 10, NULL);

    pdunet_visit(net, NULL, _visit_func, &visit_list);
    assert_int_equal(vector_len(&visit_list), 2);
    assert_string_equal(vector_at(&visit_list, 0, NULL), "ONE_RX");
    assert_string_equal(vector_at(&visit_list, 1, NULL), "ONE_TX");

    vector_reset(&visit_list);
}


void __ncodec_trace_log__(void* nc, NCodecTraceLogLevel level, const char* msg)
{
    if (level >= __log_level__) __log2console(level, NULL, 0, "%s", msg);
}

void test_pdunet_tx_fr(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    int       len;
    NCodecPdu pdu;

    // Setup the mock.
    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);

    // Configure the model (and stub the BusModel).
    simmock_setup(mock, "scalar", "network");
    NCODEC* nc = d->nc = model->sv_network->ncodec[0];
    assert_non_null(nc);
    d->bm_save = ((ABCodecInstance*)nc)->reader.bus_model;
    ((ABCodecInstance*)nc)->reader.bus_model = (ABCodecBusModel){};
    ((NCodecInstance*)nc)->trace.log = __ncodec_trace_log__;

    // Configure the PDU Net.
    PduNetworkDesc* net = pdunet_find(model->mi, nc);
    assert_non_null(net);

    // Set some signals
    PduObject* o_pdu = vector_at(&net->matrix.pdu, 1, NULL);
    assert_non_null(o_pdu);
    assert_int_equal(o_pdu->needs_tx, false);
    assert_int_equal(o_pdu->ncodec.pdu.id, 101);
    assert_int_equal(o_pdu->ncodec.pdu.payload_len, 64);
    assert_int_equal(o_pdu->pdu->id, 101);
    assert_int_equal(o_pdu->pdu->dir, PduDirectionTx);
    model->sv_signal->scalar[0] = 24;
    model->sv_signal->scalar[2] = 42;

    // Disable the schedule for this pdu.
    o_pdu->schedule.interval = 0;
    o_pdu->schedule.phase = 0;

    // Send vtable.config(), but no PDUs have needs_tx set, rx armed.
    // __log_level__ = 0;
    pdunet_tx(net, NULL, NULL, NULL, 0);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    assert_int_equal(pdu.ecu_id, 5);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(pdu.transport.flexray.metadata_type,
        NCodecPduFlexrayMetadataTypeConfig);
    // Rx armed.
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, 0);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(
        pdu.transport.flexray.metadata_type, NCodecPduFlexrayMetadataTypeLpdu);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.status,
        NCodecPduFlexrayLpduStatusNotReceived);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.frame_config_index, 1);
    // End
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Send vtable.lpdu(), set needs_tx, PDUs sent.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    uint8_t expect[] = { 26 /* 18 + 1 + 1 */, 42 };
    assert_memory_equal(pdu.payload, expect, 2);
    assert_int_equal(pdu.ecu_id, 5);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(
        pdu.transport.flexray.metadata_type, NCodecPduFlexrayMetadataTypeLpdu);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.status,
        NCodecPduFlexrayLpduStatusNotTransmitted);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
}


void _visit_count_update_flag(PduNetworkDesc* net, PduObject* pdu, void* data)
{
    assert_non_null(net);
    assert_non_null(data);
    int* counter = data;
    if (pdu->update_signals == true) {
        (*counter)++;
    }
}

void _visit_count_csum_set(PduNetworkDesc* net, PduObject* pdu, void* data)
{
    assert_non_null(net);
    assert_non_null(data);
    int* counter = data;
    if (pdu->checksum != 0) {
        (*counter)++;
    }
}

void test_pdunet_rx_fr(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    int       len;
    NCodecPdu pdu;
    int       rx_count;

    // Setup the mock.
    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);

    // Configure the model (and stub the BusModel).
    simmock_setup(mock, "scalar", "network");
    NCODEC* nc = d->nc = model->sv_network->ncodec[0];
    assert_non_null(nc);
    d->bm_save = ((ABCodecInstance*)nc)->reader.bus_model;
    ((ABCodecInstance*)nc)->reader.bus_model = (ABCodecBusModel){};
    ((NCodecInstance*)nc)->trace.log = __ncodec_trace_log__;

    // Configure the PDU Net.
    PduNetworkDesc* net = pdunet_find(model->mi, nc);
    assert_non_null(net);

    // Recv, non-FlexRay is discarded.
    pdunet_visit(net, NULL, pdunet_visit_clear_update_flag, NULL);
    ncodec_truncate(nc);
    ncodec_write(nc, &(struct NCodecPdu){
                         .id = 101,
                         .payload = (const uint8_t*)"\x18\x2a",
                         .payload_len = 2,
                         .transport_type = NCodecPduTransportTypeCan,
                     });
    ncodec_flush(nc);
    rx_count = 0;
    pdunet_rx(net, NULL, _visit_count_update_flag, &rx_count);
    assert_int_equal(rx_count, 0);


    // Recv, status is discarded, payload is decoded.
    pdunet_visit(net, NULL, pdunet_visit_clear_update_flag, NULL);
    ncodec_truncate(nc);
    ncodec_write(nc, &(struct NCodecPdu){
                         .transport_type = NCodecPduTransportTypeFlexray,
                         .transport.flexray.metadata_type =
                             NCodecPduFlexrayMetadataTypeStatus,
                     });
    ncodec_write(nc,
        &(struct NCodecPdu){
            .id = 101,
            .payload = (const uint8_t*)"\x18\x2a",
            .payload_len = 2,
            .transport_type = NCodecPduTransportTypeFlexray,
            .transport.flexray.metadata_type = NCodecPduFlexrayMetadataTypeLpdu,
        });
    ncodec_flush(nc);
    rx_count = 0;
    // Flag is set _during_ rx.
    pdunet_rx(net, NULL, _visit_count_update_flag, &rx_count);
    assert_int_equal(rx_count, 1);
    PduObject* o_pdu = vector_at(&net->matrix.pdu, 0, NULL);
    assert_non_null(o_pdu);
    // Flag was cleared _after_ rx.
    assert_int_equal(o_pdu->update_signals, false);
    assert_int_equal(o_pdu->ncodec.pdu.id, 101);
    assert_int_equal(o_pdu->ncodec.pdu.payload_len, 64);
    uint8_t expect[] = { 24, 42 };
    assert_memory_equal(o_pdu->ncodec.pdu.payload, expect, 2);
    assert_int_equal(o_pdu->pdu->id, 101);
    assert_int_equal(o_pdu->pdu->dir, PduDirectionRx);
    assert_double_equal(model->sv_signal->scalar[1], 24, 0);
    assert_double_equal(model->sv_signal->scalar[3], 42, 0);
    rx_count = 0;
    // Flag remains cleared _after_ rx.
    pdunet_rx(net, NULL, _visit_count_update_flag, &rx_count);
    assert_int_equal(rx_count, 0);
}


void test_pdunet_schedule_pdu_fr(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    int       len;
    NCodecPdu pdu;

    // Setup the mock.
    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);

    // Configure the model (and stub the BusModel).
    simmock_setup(mock, "scalar", "network");
    NCODEC* nc = d->nc = model->sv_network->ncodec[0];
    assert_non_null(nc);
    d->bm_save = ((ABCodecInstance*)nc)->reader.bus_model;
    ((ABCodecInstance*)nc)->reader.bus_model = (ABCodecBusModel){};
    ((NCodecInstance*)nc)->trace.log = __ncodec_trace_log__;

    // Configure the PDU Net.
    PduNetworkDesc* net = pdunet_find(model->mi, nc);
    assert_non_null(net);

    // Set some signals
    PduObject* o_pdu = vector_at(&net->matrix.pdu, 1, NULL);
    assert_non_null(o_pdu);
    assert_int_equal(o_pdu->needs_tx, false);
    assert_int_equal(o_pdu->ncodec.pdu.id, 101);
    assert_int_equal(o_pdu->ncodec.pdu.payload_len, 64);
    assert_int_equal(o_pdu->pdu->id, 101);
    assert_int_equal(o_pdu->pdu->dir, PduDirectionTx);
    model->sv_signal->scalar[0] = 24;
    model->sv_signal->scalar[2] = 42;

    // Send vtable.config(), but no PDUs have needs_tx set.
    pdunet_tx(net, NULL, NULL, NULL, 0);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    assert_int_equal(pdu.ecu_id, 5);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(pdu.transport.flexray.metadata_type,
        NCodecPduFlexrayMetadataTypeConfig);
    // Rx armed.
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, 0);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(
        pdu.transport.flexray.metadata_type, NCodecPduFlexrayMetadataTypeLpdu);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.status,
        NCodecPduFlexrayLpduStatusNotReceived);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.frame_config_index, 1);
    // End
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Count set checksums.
    int count = 0;
    pdunet_visit(net, NULL, _visit_count_csum_set, &count);
    assert_int_equal(count, 1);

    // Push the simulation to interval + phase - 1 step. No Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.0035);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation one step. Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.004);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    assert_true(len > 0);
    uint8_t expect[] = { 25 /* 18 + 1 */, 42 };
    assert_memory_equal(pdu.payload, expect, 2);
    assert_int_equal(pdu.ecu_id, 5);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation one step. No Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.0045);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation to interval + phase - 1 step. No Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.0085);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation one step. Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.009);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    assert_true(len > 0);
    uint8_t expect2[] = { 26 /* 18 + 1 + 1 */, 42 };
    assert_memory_equal(pdu.payload, expect2, 2);
    assert_int_equal(pdu.ecu_id, 5);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation one step. No Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.0095);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
}


void test_pdunet_schedule_net_fr(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    int       len;
    NCodecPdu pdu;

    // Setup the mock.
    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);

    // Configure the model (and stub the BusModel).
    simmock_setup(mock, "scalar", "network");
    NCODEC* nc = d->nc = model->sv_network->ncodec[0];
    assert_non_null(nc);
    d->bm_save = ((ABCodecInstance*)nc)->reader.bus_model;
    ((ABCodecInstance*)nc)->reader.bus_model = (ABCodecBusModel){};
    ((NCodecInstance*)nc)->trace.log = __ncodec_trace_log__;

    // Configure the PDU Net.
    PduNetworkDesc* net = pdunet_find(model->mi, nc);
    assert_non_null(net);

    // Set some signals
    PduObject* o_pdu = vector_at(&net->matrix.pdu, 1, NULL);
    assert_non_null(o_pdu);
    assert_int_equal(o_pdu->needs_tx, false);
    assert_int_equal(o_pdu->ncodec.pdu.id, 101);
    assert_int_equal(o_pdu->ncodec.pdu.payload_len, 64);
    assert_int_equal(o_pdu->pdu->id, 101);
    assert_int_equal(o_pdu->pdu->dir, PduDirectionTx);
    model->sv_signal->scalar[0] = 24;
    model->sv_signal->scalar[2] = 42;

    // Send vtable.config(), but no PDUs have needs_tx set.
    pdunet_tx(net, NULL, NULL, NULL, 0);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    assert_int_equal(pdu.ecu_id, 5);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(pdu.transport.flexray.metadata_type,
        NCodecPduFlexrayMetadataTypeConfig);
    // Rx armed.
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, 0);
    assert_int_equal(pdu.transport_type, NCodecPduTransportTypeFlexray);
    assert_int_equal(
        pdu.transport.flexray.metadata_type, NCodecPduFlexrayMetadataTypeLpdu);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.status,
        NCodecPduFlexrayLpduStatusNotReceived);
    assert_int_equal(pdu.transport.flexray.metadata.lpdu.frame_config_index, 1);
    // End
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Count set checksums.
    int count = 0;
    pdunet_visit(net, NULL, _visit_count_csum_set, &count);
    assert_int_equal(count, 1);

    // Set the epoch_offset
    net->schedule.epoch_offset = 0.002;

    // Push the simulation to offset + interval + phase - 1 step. No Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.0055);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation one step. Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.006);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    pdu = (NCodecPdu){ 0 };
    len = ncodec_read(nc, &pdu);
    assert_true(len > 0);
    uint8_t expect[] = { 25 /* 18 + 1 */, 42 };
    assert_memory_equal(pdu.payload, expect, 2);
    assert_int_equal(pdu.ecu_id, 5);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);

    // Push the simulation one step. No Tx.
    pdunet_tx(net, NULL, pdunet_visit_needs_tx, NULL, 0.0065);
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
}


void test_pdunet_schedule_status_fr(void** state)
{
    chdir(PDUNET_MODEL_PATH);
    TestData* d = *state;

    int       len;
    NCodecPdu pdu;
    int       rx_count;

    // Setup the mock.
    SimMock* mock = d->mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(
        mock, inst_argv, ARRAY_SIZE(inst_argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, PDUNET_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);

    // Configure the model (and stub the BusModel).
    simmock_setup(mock, "scalar", "network");
    NCODEC* nc = d->nc = model->sv_network->ncodec[0];
    assert_non_null(nc);
    d->bm_save = ((ABCodecInstance*)nc)->reader.bus_model;
    ((ABCodecInstance*)nc)->reader.bus_model = (ABCodecBusModel){};
    ((NCodecInstance*)nc)->trace.log = __ncodec_trace_log__;

    // Configure the PDU Net.
    PduNetworkDesc* net = pdunet_find(model->mi, nc);
    assert_non_null(net);
    net->schedule.simulation_time = 4; /* 2mS */
    net->network.vtable.flexray.cycle = 1;

    // Recv, status - cycle set.
    pdunet_visit(net, NULL, pdunet_visit_clear_update_flag, NULL);
    ncodec_truncate(nc);
    ncodec_write(nc, &(struct NCodecPdu){
                         .transport_type = NCodecPduTransportTypeFlexray,
                         .transport.flexray.metadata_type =
                             NCodecPduFlexrayMetadataTypeStatus,
                         .transport.flexray.metadata.status = { .cycle = 1,
                             .macrotick = 600 },
                     });
    ncodec_flush(nc);
    pdunet_rx(net, NULL, NULL, NULL);
    assert_double_equal(net->schedule.epoch_offset, 0.000, 0.0);

    // Recv, status - cycle change, should set epoch_offset to align.
    pdunet_visit(net, NULL, pdunet_visit_clear_update_flag, NULL);
    ncodec_truncate(nc);
    ncodec_write(nc, &(struct NCodecPdu){
                         .transport_type = NCodecPduTransportTypeFlexray,
                         .transport.flexray.metadata_type =
                             NCodecPduFlexrayMetadataTypeStatus,
                         .transport.flexray.metadata.status = { .cycle = 2,
                             .macrotick = 30 },
                     });
    ncodec_flush(nc);
    pdunet_rx(net, NULL, NULL, NULL);
    assert_double_equal(net->schedule.epoch_offset, 0.002, 0.0);
}


int run_pdunet_ncodec_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    getcwd(__entry_path__, PATH_MAX);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_pdunet_setup, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_find, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_visit, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_tx_fr, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_rx_fr, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_schedule_pdu_fr, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_schedule_net_fr, s, t),
        cmocka_unit_test_setup_teardown(test_pdunet_schedule_status_fr, s, t),
    };

    return cmocka_run_group_tests_name("MODEL / PDUNET", tests, NULL, NULL);
}
