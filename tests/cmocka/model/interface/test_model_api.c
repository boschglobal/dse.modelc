// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <unistd.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/mocks/simmock.h>


extern ModelSignalIndex __model_index__(
    ModelDesc* m, const char* vname, const char* sname);

static char __entry_path__[PATH_MAX];


static int test_setup(void** state)
{
    UNUSED(state);
    return 0;
}


static int test_teardown(void** state)
{
    SimMock* mock = *state;

    simmock_exit(mock, true);
    simmock_free(mock);

    chdir(__entry_path__);

    return 0;
}


#define BINARY_INST_NAME      "binary_inst"
#define BINARY_SIGNAL_COUNTER 0
#define BINARY_SIGNAL_MESSAGE 0


typedef struct _index_check {
    const char*      vname;
    const char*      sname;
    /* Expected. */
    ModelSignalIndex msi;
} _index_check;

void test_model_api__model_index(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/binary");

    const char* inst_names[] = {
        BINARY_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_api",
        (char*)"--name=" BINARY_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"data/simulation.yaml",
        (char*)"data/model.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, BINARY_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, true);
    simmock_setup(mock, "scalar_channel", "binary_channel");


    simmock_print_scalar_signals(mock, LOG_DEBUG);
    simmock_print_binary_signals(mock, LOG_DEBUG);


    _index_check tc[] = {
        /* Signal matching. */
        { .vname = "scalar", .sname = "counter", .msi = {
            .sv = model->sv_signal,
            .scalar = &model->sv_signal->scalar[0],
            .binary = NULL,
            .vector = 0,
            .signal = 0,
            },
        },
        { .vname = "binary", .sname = "message", .msi = {
            .sv = model->sv_network,
            .scalar = NULL,
            .binary = &model->sv_network->binary[0],
            .vector = 1,
            .signal = 0,
            },
        },
        /* Vector match only. */
        { .vname = "scalar", .sname = NULL, .msi = {
            .sv = model->sv_signal,
            .scalar = NULL,
            .binary = NULL,
            .vector = 0,
            .signal = 0,
            },
        },
        { .vname = "binary", .sname = NULL, .msi = {
            .sv = model->sv_network,
            .scalar = NULL,
            .binary = NULL,
            .vector = 1,
            .signal = 0,
            },
        },
    };

    assert_non_null(model->mi->model_desc);
    assert_non_null(model->mi->model_desc->sv);
    for (uint i = 0; i < ARRAY_SIZE(tc); i++) {
        ModelSignalIndex msi =
            __model_index__(model->mi->model_desc, tc[i].vname, tc[i].sname);
        assert_ptr_equal(msi.sv, tc[i].msi.sv);
        assert_ptr_equal(msi.scalar, tc[i].msi.scalar);
        assert_ptr_equal(msi.binary, tc[i].msi.binary);
        assert_int_equal(msi.vector, tc[i].msi.vector);
        assert_int_equal(msi.signal, tc[i].msi.signal);
    }
}


#define MINIMAL_INST_NAME      "minimal_inst"

void test_model_api__model_annotation(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/minimal");

    const char* inst_names[] = {
        MINIMAL_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_api",
        (char*)"--name=" MINIMAL_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"../../../../../../tests/cmocka/build/_out/resources/model/annotations.yaml",
        (char*)"" // FIXME getopt_long will seg if only one file argument.
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, MINIMAL_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, false, true, false);
    simmock_setup(mock, "scalar_channel", "binary_channel");

    const char* val = NULL;

    /* Model annotations. */
    val = model_annotation(model->mi->model_desc, "note");
    assert_non_null(val);
    assert_string_equal(val, "model_annotation");
    val = model_annotation(model->mi->model_desc, "nest/note");
    assert_non_null(val);
    assert_string_equal(val, "model_annotation_nest");

    /* Model Instance annotations. */
    val = model_instance_annotation(model->mi->model_desc, "note");
    assert_non_null(val);
    assert_string_equal(val, "instance_annotation");
    val = model_instance_annotation(model->mi->model_desc, "nest/note");
    assert_non_null(val);
    assert_string_equal(val, "instance_annotation_nest");
}


#define BINARY_INST_NAME       "binary_inst"
#define BINARY_SIGNAL_MESSAGE  0
#define BINARY_SIGNAL_NOT_USED 1

void test_model_api__binary_stream_reset(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/binary");

    const char* inst_names[] = {
        BINARY_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_api",
        (char*)"--name=" BINARY_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"../../../../../../tests/cmocka/build/_out/resources/model/binary_stream_reset.yaml",
        (char*)"data/model.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, BINARY_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, true);
    simmock_setup(mock, "scalar_channel", "binary_channel");

    double   counter = 42.0;
    char     buffer[100] = "";
    uint32_t len = 0;

    simmock_print_binary_signals(mock, LOG_DEBUG);

    /* T0 ... Tn */
    for (uint32_t i = 0; i < 5; i++) {
        /* Do the check. */
        BinaryCheck b_checks[] = {
            { .index = BINARY_SIGNAL_MESSAGE,
                .buffer = (uint8_t*)buffer,
                .len = len },
            /* Len should be 0, buffer would not matter. */
            { .index = BINARY_SIGNAL_NOT_USED,
                .buffer = (uint8_t*)buffer,
                .len = 0 },
        };
        simmock_binary_check(
            mock, BINARY_INST_NAME, b_checks, ARRAY_SIZE(b_checks), NULL);
        /* Inject some binary data, if reset is called, this will be ignored. */
        SignalVector* sv = mock->sv_network_tx;
        char foobar[100] = "foobar";
        for (int j = 0; j < 2; j++) {
            sv->vtable.reset(sv, j);
            sv->vtable.append(sv, j, foobar, strlen(foobar) + 1);
        }
        /* Step the model. */
        assert_int_equal(simmock_step(mock, true), 0);
        sv = model->sv_network;
        assert_true(sv->reset_called[0]);
        assert_false(sv->reset_called[1]);
        assert_int_equal(sv->length[1], 0);
        simmock_print_binary_signals(mock, LOG_DEBUG);
        /* Set check conditions (for next step). */
        counter += 1.0;
        snprintf(buffer, sizeof(buffer), "count is %d", (int)counter);
        len = strlen(buffer) + 1;
    }
}


int run_model_api_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    getcwd(__entry_path__, PATH_MAX);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_model_api__model_index, s, t),
        cmocka_unit_test_setup_teardown(test_model_api__model_annotation, s, t),
        cmocka_unit_test_setup_teardown(test_model_api__binary_stream_reset, s, t),
    };

    return cmocka_run_group_tests_name("MODEL / API", tests, NULL, NULL);
}
