// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/ncodec/codec.h>


#define UNUSED(x)         ((void)x)
#define ARRAY_SIZE(x)     (sizeof((x)) / sizeof((x)[0]))
#define BUF_NODEID_OFFSET 53
#define PDU_SIG_IDX       3

extern uint8_t __log_level__;

typedef struct ModelCMock {
    SimulationSpec     sim;
    ModelInstanceSpec* mi;
} ModelCMock;


static uint _sv_count(SignalVector* sv)
{
    uint count = 0;
    while (sv && sv->name) {
        count++;
        /* Next signal vector. */
        sv++;
    }
    return count;
}


static int _sv_nop(ModelDesc* model, double* model_time, double stop_time)
{
    UNUSED(model);
    UNUSED(model_time);
    UNUSED(stop_time);
    return 0;
}


static int test_setup(void** state)
{
    ModelCMock* mock = calloc(1, sizeof(ModelCMock));
    assert_non_null(mock);

    int             rc;
    ModelCArguments args;
    char*           argv[] = {
        (char*)"test_signal",
        (char*)"--name=signal",
        (char*)"resources/model/ncodec.yaml",
    };

    modelc_set_default_args(&args, "test", 0.005, 0.005);
    args.log_level = __log_level__;
    modelc_parse_arguments(&args, ARRAY_SIZE(argv), argv, "Signal");
    rc = modelc_configure(&args, &mock->sim);
    assert_int_equal(rc, 0);
    mock->mi = modelc_get_model_instance(&mock->sim, args.name);
    assert_non_null(mock->mi);
    ModelVTable vtable = { .step = _sv_nop };
    rc = modelc_model_create(&mock->sim, mock->mi, &vtable);
    assert_int_equal(rc, 0);

    /* Return the mock. */
    *state = mock;
    return 0;
}


static int test_teardown(void** state)
{
    ModelCMock* mock = *state;

    if (mock && mock->mi) {
        dse_yaml_destroy_doc_list(mock->mi->yaml_doc_list);
    }
    if (mock) {
        modelc_exit(&mock->sim);
        free(mock);
    }

    return 0;
}


void test_ncodec_pdu__ncodec_open(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);

    /* Use the "binary" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* Check the ncodec objects. */
    assert_null(signal_codec(sv, 0));
    assert_null(signal_codec(sv, 1));
    assert_non_null(signal_codec(sv, PDU_SIG_IDX));
    assert_null(sv->ncodec[0]);
    assert_null(sv->ncodec[1]);
    assert_non_null(sv->ncodec[PDU_SIG_IDX]);
    assert_ptr_equal(signal_codec(sv, PDU_SIG_IDX), sv->ncodec[PDU_SIG_IDX]);
}


void test_ncodec_pdu__read_empty(void** state)
{
    ModelCMock* mock = *state;
    size_t      len;
    NCodecPdu   pdu;

    /* Use the "binary" signal vector. */
    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }
    assert_non_null(signal_codec(sv, PDU_SIG_IDX));
    NCODEC* nc = signal_codec(sv, PDU_SIG_IDX);

    /* First read. */
    memset(&pdu, 0, sizeof(NCodecPdu));
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
    assert_int_equal(pdu.payload_len, 0);
    assert_null(pdu.payload);

    /* Read after reset. */
    memset(&pdu, 0, sizeof(NCodecPdu));
    signal_reset(sv, PDU_SIG_IDX);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
    assert_int_equal(pdu.payload_len, 0);
    assert_null(pdu.payload);

    /* Read after release. */
    memset(&pdu, 0, sizeof(NCodecPdu));
    signal_release(sv, PDU_SIG_IDX);
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
    assert_int_equal(pdu.payload_len, 0);
    assert_null(pdu.payload);
}


void test_ncodec_pdu__network_stream(void** state)
{
    ModelCMock* mock = *state;
    int         rc;

    /* Use the "binary" signal vector. */
    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* Check the ncodec objects. */
    assert_non_null(signal_codec(sv, PDU_SIG_IDX));
    assert_non_null(sv->ncodec[PDU_SIG_IDX]);
    assert_ptr_equal(signal_codec(sv, PDU_SIG_IDX), sv->ncodec[PDU_SIG_IDX]);

    /* Use the codec object with the ncodec library. */
    const char* greeting = "Hello PDU World";
    NCODEC*     nc = signal_codec(sv, PDU_SIG_IDX);
    signal_reset(sv, PDU_SIG_IDX);
    rc = ncodec_write(nc, &(struct NCodecPdu){
                              .id = 42,
                              .payload = (uint8_t*)greeting,
                              .payload_len = strlen(greeting),
                              .swc_id = 88  // bypass RX filter.
                          });
    assert_int_equal(rc, strlen(greeting));
    size_t len = ncodec_flush(nc);
    assert_int_equal(len, 0x5a);
    assert_int_equal(len, sv->length[PDU_SIG_IDX]);

    /* Copy message, keeping the content. */
    uint8_t buffer[1024];
    memcpy(buffer, sv->binary[PDU_SIG_IDX], len);
    // for (uint32_t i = 0; i< buffer_len;i+=8) printf("%02x %02x %02x %02x %02x
    // %02x %02x %02x\n",
    //     buffer[i+0], buffer[i+1], buffer[i+2], buffer[i+3],
    //     buffer[i+4], buffer[i+5], buffer[i+6], buffer[i+7]);

    /* Read the message back. */
    signal_release(sv, PDU_SIG_IDX);
    signal_append(sv, PDU_SIG_IDX, buffer, len);
    NCodecPdu pdu = {};
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, strlen(greeting));
    assert_int_equal(pdu.payload_len, strlen(greeting));
    assert_non_null(pdu.payload);
    assert_memory_equal(pdu.payload, greeting, strlen(greeting));
    assert_int_equal(pdu.swc_id, 88);
    assert_int_equal(pdu.ecu_id, 5);
}


void test_ncodec_pdu__truncate(void** state)
{
    ModelCMock* mock = *state;
    int         rc;

    /* Use the "binary" signal vector. */
    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* Check the ncodec objects. */
    assert_non_null(signal_codec(sv, PDU_SIG_IDX));
    assert_non_null(sv->ncodec[PDU_SIG_IDX]);
    assert_ptr_equal(signal_codec(sv, PDU_SIG_IDX), sv->ncodec[PDU_SIG_IDX]);

    /* Use the codec object with the ncodec library. */
    const char*     greeting = "Hello PDU World";
    NCODEC*         nc = signal_codec(sv, PDU_SIG_IDX);
    NCodecInstance* _nc = (NCodecInstance*)nc;

    signal_reset(sv, PDU_SIG_IDX);
    rc = ncodec_write(nc, &(struct NCodecPdu){
                              .id = 42,
                              .payload = (uint8_t*)greeting,
                              .payload_len = strlen(greeting),
                              .swc_id = 88  // bypass RX filter.
                          });
    assert_int_equal(rc, strlen(greeting));
    size_t len = ncodec_flush(nc);
    assert_int_equal(len, 0x5a);
    assert_int_equal(len, sv->length[PDU_SIG_IDX]);
    assert_int_equal(0x5a, _nc->stream->tell(nc));

    /* Truncate the NCodec, and check underlying stream/sv. */
    uint32_t _bs = sv->buffer_size[PDU_SIG_IDX];
    void*    _bin = sv->binary[PDU_SIG_IDX];
    ncodec_truncate(nc);
    /* Check  sv properties. */
    assert_int_equal(0, sv->length[PDU_SIG_IDX]);
    assert_int_equal(_bs, sv->buffer_size[PDU_SIG_IDX]);
    assert_ptr_equal(_bin, sv->binary[PDU_SIG_IDX]);
    /* Check  stream properties. */
    assert_int_equal(0, _nc->stream->tell(nc));
}


void test_ncodec_pdu__config(void** state)
{
    ModelCMock* mock = *state;

    /* Use the "binary" signal vector. */
    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }
    assert_non_null(signal_codec(sv, PDU_SIG_IDX));
    NCODEC* nc = signal_codec(sv, PDU_SIG_IDX);
    ncodec_truncate(nc);  // ModelC calls reset() internally.

    /* Adjust the swc_id. */
    ncodec_config(nc, (struct NCodecConfigItem){
                          .name = "swc_id",
                          .value = "88",
                      });

    /* Write a message, and check the emitted swc_id. */
    const char* greeting = "Hello PDU World";
    ncodec_write(nc, &(struct NCodecPdu){
                         .id = 42,
                         .payload = (uint8_t*)greeting,
                         .payload_len = strlen(greeting),
                         .swc_id = 88  // Should be filtered.
                     });
    size_t len = ncodec_flush(nc);
    assert_int_equal(len, 0x5a);
    assert_int_equal(len, sv->length[PDU_SIG_IDX]);

    /* Copy message, keeping the content. */
    uint8_t buffer[1024];
    memcpy(buffer, sv->binary[PDU_SIG_IDX], len);
    // for (uint32_t i = 0; i< buffer_len;i+=8) printf("%02x %02x %02x %02x %02x
    // %02x %02x %02x\n",
    //     buffer[i+0], buffer[i+1], buffer[i+2], buffer[i+3],
    //     buffer[i+4], buffer[i+5], buffer[i+6], buffer[i+7]);

    /* Read the message back. */
    signal_release(sv, PDU_SIG_IDX);
    signal_append(sv, PDU_SIG_IDX, buffer, len);
    NCodecPdu pdu = {};
    len = ncodec_read(nc, &pdu);
    assert_int_equal(len, -ENOMSG);
}


static void _adjust_swc_id(NCODEC* nc, const char* swc_id)
{
    ncodec_config(nc, (struct NCodecConfigItem){
                          .name = "swc_id",
                          .value = swc_id,
                      });
}

void test_ncodec_pdu__call_sequence(void** state)
{
    ModelCMock* mock = *state;

    /* Use the "binary" signal vector. */
    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* Check the ncodec objects. */
    assert_non_null(signal_codec(sv, PDU_SIG_IDX));
    assert_non_null(sv->ncodec[PDU_SIG_IDX]);
    assert_ptr_equal(signal_codec(sv, PDU_SIG_IDX), sv->ncodec[PDU_SIG_IDX]);

    /* Initial conditions. */
    const char* greeting = "Hello PDU World";
    NCODEC*     nc = signal_codec(sv, PDU_SIG_IDX);

    assert_int_equal(0, ncodec_seek(nc, 0, NCODEC_SEEK_END));
    assert_int_equal(0, ncodec_tell(nc));
    _adjust_swc_id(nc, "42");
    ncodec_truncate(nc);  // ModelC calls reset() internally.
    size_t len = ncodec_write(nc, &(struct NCodecPdu){ .id = 42,
                                      .payload = (uint8_t*)greeting,
                                      .payload_len = strlen(greeting) });
    assert_int_equal(strlen(greeting), len);
    ncodec_flush(nc);
    _adjust_swc_id(nc, "4");
#define EXPECT_POS 0x5a
    assert_int_equal(EXPECT_POS, ncodec_tell(nc));

    /* Read then Write. */
    for (int i = 0; i < 5; i++) {
        /* Seek - position to start of stream. */
        assert_int_equal(EXPECT_POS, ncodec_seek(nc, 0, NCODEC_SEEK_END));
        ncodec_seek(nc, 0, NCODEC_SEEK_SET);
        assert_int_equal(0, ncodec_tell(nc));
        /* Read - consume the stream. */
        NCodecPdu pdu = {};
        size_t    len = ncodec_read(nc, &pdu);
        assert_int_equal(strlen(greeting), len);
        /* Truncate - reset the stream. */
        ncodec_truncate(nc);
        assert_int_equal(0, ncodec_tell(nc));
        assert_int_equal(0, ncodec_seek(nc, 0, NCODEC_SEEK_END));
        assert_int_equal(0, ncodec_tell(nc));
        /* Write and Flush - write to stream (spoof swc_id). */
        _adjust_swc_id(nc, "42");
        len = ncodec_write(nc, &(struct NCodecPdu){ .id = 42,
                                   .payload = (uint8_t*)greeting,
                                   .payload_len = strlen(greeting) });
        assert_int_equal(strlen(greeting), len);
        ncodec_flush(nc);
        _adjust_swc_id(nc, "4");
        assert_int_equal(EXPECT_POS, ncodec_tell(nc));
        assert_int_equal(EXPECT_POS, ncodec_seek(nc, 0, NCODEC_SEEK_END));
    }
}


int run_ncodec_pdu_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_ncodec_pdu__ncodec_open, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec_pdu__read_empty, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec_pdu__network_stream, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec_pdu__truncate, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec_pdu__config, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec_pdu__call_sequence, s, t),
    };

    return cmocka_run_group_tests_name("NCODEC PDU", tests, NULL, NULL);
}
