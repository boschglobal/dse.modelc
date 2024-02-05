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


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#define BUF_NODEID_OFFSET 53


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


void test_ncodec__ncodec_open(void** state)
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
    assert_null(sv->codec(sv, 0));
    assert_null(sv->codec(sv, 1));
    assert_non_null(sv->codec(sv, 2));
    assert_null(sv->ncodec[0]);
    assert_null(sv->ncodec[1]);
    assert_non_null(sv->ncodec[2]);
    assert_ptr_equal(sv->codec(sv, 2), sv->ncodec[2]);
}


void test_ncodec__read_empty(void** state)
{
    ModelCMock*   mock = *state;
    size_t        len;
    NCodecCanMessage msg;

    /* Use the "binary" signal vector. */
    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }
    assert_non_null(sv->codec(sv, 2));
    NCODEC* nc = sv->codec(sv, 2);

    /* First read. */
    memset(&msg, 0, sizeof(NCodecCanMessage));
    len = ncodec_read(nc, &msg);
    assert_int_equal(len, -ENOMSG);
    assert_int_equal(msg.len, 0);
    assert_null(msg.buffer);

    /* Read after reset. */
    memset(&msg, 0, sizeof(NCodecCanMessage));
    sv->reset(sv, 2);
    len = ncodec_read(nc, &msg);
    assert_int_equal(len, -ENOMSG);
    assert_int_equal(msg.len, 0);
    assert_null(msg.buffer);

    /* Read after release. */
    memset(&msg, 0, sizeof(NCodecCanMessage));
    sv->release(sv, 2);
    len = ncodec_read(nc, &msg);
    assert_int_equal(len, -ENOMSG);
    assert_int_equal(msg.len, 0);
    assert_null(msg.buffer);
}


void test_ncodec__network_stream(void** state)
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
    assert_non_null(sv->codec(sv, 2));
    assert_non_null(sv->ncodec[2]);
    assert_ptr_equal(sv->codec(sv, 2), sv->ncodec[2]);

    /* Use the codec object with the ncodec library. */
    const char* greeting = "Hello World";
    NCODEC*     nc = sv->codec(sv, 2);
    sv->reset(sv, 2);
    rc = ncodec_write(nc, &(struct NCodecCanMessage){ .frame_id = 42,
                              .buffer = (uint8_t*)greeting,
                              .len = strlen(greeting) });
    assert_int_equal(rc, strlen(greeting));
    size_t len = ncodec_flush(nc);
    assert_int_equal(len, 0x66);
    assert_int_equal(len, sv->length[2]);

    /* Copy message, keeping the content, modify the node_id. */
    uint8_t buffer[1024];
    memcpy(buffer, sv->binary[2], len);
    buffer[BUF_NODEID_OFFSET] = 8;
    // for (uint32_t i = 0; i< buffer_len;i+=8) printf("%02x %02x %02x %02x %02x
    // %02x %02x %02x\n",
    //     buffer[i+0], buffer[i+1], buffer[i+2], buffer[i+3],
    //     buffer[i+4], buffer[i+5], buffer[i+6], buffer[i+7]);

    /* Read the message back. */
    sv->release(sv, 2);
    sv->append(sv, 2, buffer, len);
    NCodecCanMessage msg = {};
    len = ncodec_read(nc, &msg);
    assert_int_equal(len, strlen(greeting));
    assert_int_equal(msg.len, strlen(greeting));
    assert_non_null(msg.buffer);
    assert_memory_equal(msg.buffer, greeting, strlen(greeting));
}


void test_ncodec__config(void** state)
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
    assert_non_null(sv->codec(sv, 2));
    NCODEC* nc = sv->codec(sv, 2);

    /* Adjust the node_id. */
    ncodec_config(nc, (struct NCodecConfigItem){
                          .name = "node_id",
                          .value = "8",
                      });

    /* Write a message, and check the emitted node_id. */
    const char* greeting = "Hello World";
    ncodec_write(nc, &(struct NCodecCanMessage){ .frame_id = 42,
                         .buffer = (uint8_t*)greeting,
                         .len = strlen(greeting) });
    size_t len = ncodec_flush(nc);
    assert_int_equal(len, 0x66);
    assert_int_equal(len, sv->length[2]);
    assert_int_equal(((uint8_t*)(sv->binary[2]))[BUF_NODEID_OFFSET], 8);
}


int run_ncodec_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_ncodec__ncodec_open, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec__read_empty, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec__network_stream, s, t),
        cmocka_unit_test_setup_teardown(test_ncodec__config, s, t),
    };

    return cmocka_run_group_tests_name("NCODEC", tests, NULL, NULL);
}
