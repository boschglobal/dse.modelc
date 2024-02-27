// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <unistd.h>
#include <stdbool.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/mocks/simmock.h>


static char __entry_path__[200];


static int test_setup(void** state)
{
    UNUSED(state);
    return 0;
}


static int test_teardown(void** state)
{
    SimMock* mock = *state;

    simmock_exit(mock, false);
    simmock_free(mock);

    chdir(__entry_path__);

    return 0;
}


#define MINIMAL_INST_NAME      "minimal_inst"
#define MINIMAL_SIGNAL_COUNTER 0

void test_model__minimal(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/minimal");

    const char* inst_names[] = {
        MINIMAL_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_interface",
        (char*)"--name=" MINIMAL_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"data/simulation.yaml",
        (char*)"data/model.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, MINIMAL_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, false, true, false);
    simmock_setup(mock, "data_channel", NULL);

    /* Initial value. */
    double counter = 0.0;
    simmock_print_scalar_signals(mock, LOG_DEBUG);
    /* T0 ... Tn */
    for (uint32_t i = 0; i < 5; i++) {
        /* Do the check. */
        SignalCheck checks[] = {
            { .index = MINIMAL_SIGNAL_COUNTER, .value = counter },
        };
        simmock_signal_check(
            mock, MINIMAL_INST_NAME, checks, ARRAY_SIZE(checks), NULL);
        /* Step the model. */
        assert_int_equal(simmock_step(mock, true), 0);
        simmock_print_scalar_signals(mock, LOG_DEBUG);
        counter += 1.0;
    }
}


#define EXTENDED_INST_NAME      "extended_inst"
#define EXTENDED_SIGNAL_COUNTER 0
#define EXTENDED_SIGNAL_ODD     1
#define EXTENDED_SIGNAL_EVEN    2

void test_model__extended(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/extended");

    const char* inst_names[] = {
        EXTENDED_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_interface",
        (char*)"--name=" EXTENDED_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"data/simulation.yaml",
        (char*)"data/model.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, EXTENDED_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);
    simmock_setup(mock, "data_channel", NULL);

    /* Initial value. */
    double counter = 42.0;
    double odd = false;
    double even = true;
    simmock_print_scalar_signals(mock, LOG_DEBUG);
    /* T0 ... Tn */
    for (uint32_t i = 0; i < 5; i++) {
        /* Do the check. */
        SignalCheck checks[] = {
            { .index = EXTENDED_SIGNAL_COUNTER, .value = counter },
            { .index = EXTENDED_SIGNAL_ODD, .value = odd },
            { .index = EXTENDED_SIGNAL_EVEN, .value = even },
        };
        simmock_signal_check(
            mock, EXTENDED_INST_NAME, checks, ARRAY_SIZE(checks), NULL);
        /* Step the model. */
        assert_int_equal(simmock_step(mock, true), 0);
        simmock_print_scalar_signals(mock, LOG_DEBUG);
        /* Set check conditions (for next step). */
        counter += 1.0;
        odd = (bool)odd ? false : true;
        even = (bool)even ? false : true;
    }
}


#define BINARY_INST_NAME      "binary_inst"
#define BINARY_SIGNAL_COUNTER 0
#define BINARY_SIGNAL_MESSAGE 0

void test_model__binary(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/binary");

    const char* inst_names[] = {
        BINARY_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_interface",
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

    /* Initial value. */
    double   counter = 42.0;
    char     buffer[100] = "";
    uint32_t len = 0;

    simmock_print_scalar_signals(mock, LOG_DEBUG);
    simmock_print_binary_signals(mock, LOG_DEBUG);
    /* T0 ... Tn */
    for (uint32_t i = 0; i < 2; i++) {
        /* Do the check. */
        SignalCheck checks[] = {
            { .index = BINARY_SIGNAL_COUNTER, .value = counter },
        };
        BinaryCheck b_checks[] = {
            { .index = BINARY_SIGNAL_MESSAGE,
                .buffer = (uint8_t*)buffer,
                .len = len },
        };
        simmock_signal_check(
            mock, BINARY_INST_NAME, checks, ARRAY_SIZE(checks), NULL);
        simmock_binary_check(
            mock, BINARY_INST_NAME, b_checks, ARRAY_SIZE(b_checks), NULL);
        /* Step the model. */
        assert_int_equal(simmock_step(mock, true), 0);
        simmock_print_scalar_signals(mock, LOG_DEBUG);
        simmock_print_binary_signals(mock, LOG_DEBUG);
        /* Set check conditions (for next step). */
        counter += 1.0;
        snprintf(buffer, sizeof(buffer), "count is %d", (int)counter);
        len = strlen(buffer) + 1;
    }
}

#define BENCHMARK_INST_NAME      "benchmark_inst"
#define BENCHMARK_SIGNAL_COUNTER 0

void test_model__benchmark(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/benchmark");

    const char* inst_names[] = {
        BENCHMARK_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_interface",
        (char*)"--name=" BENCHMARK_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"data/simulation.yaml",
        (char*)"data/model.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, BENCHMARK_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, false, true, false);
    simmock_setup(mock, "data_channel", NULL);

    /* Initial value. */
    double counter = 0.0;
    simmock_print_scalar_signals(mock, LOG_DEBUG);
    /* T0 ... Tn */
    for (uint32_t i = 0; i < 5; i++) {
        /* Do the check. */
        SignalCheck checks[] = {
            { .index = BENCHMARK_SIGNAL_COUNTER, .value = counter },
        };
        simmock_signal_check(
            mock, BENCHMARK_INST_NAME, checks, ARRAY_SIZE(checks), NULL);
        /* Step the model. */
        assert_int_equal(simmock_step(mock, true), 0);
        simmock_print_scalar_signals(mock, LOG_DEBUG);
        counter += 1.2;
    }
}


#define NCODEC_INST_NAME      "ncodec_inst"
#define NCODEC_SIGNAL_COUNTER 0
#define NCODEC_SIGNAL_MESSAGE 0
#define CAN_FRAME_LENGTH 64

void test_model__ncodec(void** state)
{
    chdir("../../../../dse/modelc/build/_out/examples/ncodec");

    const char* inst_names[] = {
        NCODEC_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_model_interface",
        (char*)"--name=" NCODEC_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"data/simulation.yaml",
        (char*)"data/model.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, NCODEC_INST_NAME);
    simmock_load(mock);
    simmock_load_model_check(model, true, true, false);
    simmock_setup(mock, NULL, "binary_channel");

    /* Initial value. */
    int counter = 1001; /* Will be frame_id for looped back frames. */

    simmock_print_binary_signals(mock, LOG_DEBUG);
    /* T0 */
    assert_int_equal(simmock_step(mock, true), 0);
    /* T1 ... Tn */
    for (uint32_t i = 0; i < 4; i++) {
        // Check the RX (sent in previous step by the Model).
        uint8_t  rx_data[CAN_FRAME_LENGTH] = {};
        uint32_t frame_id = simmock_read_frame(
            model->sv_network, "can", rx_data, CAN_FRAME_LENGTH);
        log_debug("Frame %d : %s", frame_id, rx_data);
        assert_int_equal(frame_id, counter);
        assert_string_equal((char*)rx_data, "Hello World! from node_id=24");
        /* Step the model. */
        assert_int_equal(simmock_step(mock, true), 0);
        simmock_print_binary_signals(mock, LOG_DEBUG);
        counter++;
    }
}


int run_model_examples_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    getcwd(__entry_path__, 200);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_model__minimal, s, t),
        cmocka_unit_test_setup_teardown(test_model__extended, s, t),
        cmocka_unit_test_setup_teardown(test_model__binary, s, t),
        cmocka_unit_test_setup_teardown(test_model__ncodec, s, t),
#ifndef _WIN32
        cmocka_unit_test_setup_teardown(test_model__benchmark, s, t),
#endif
    };

    return cmocka_run_group_tests_name("MODEL / EXAMPLES", tests, NULL, NULL);
}
