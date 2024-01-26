#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/mocks/simmock.h>

static int test_setup(void** state)
{
    const char* inst_names[] = {
        "target_inst",
        "network_inst",
    };
    char* argv[] = {
        (char*)"test_runnable",
        (char*)"--name=target_inst;network_inst",
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"../../../../tests/cmocka/network/stack.yaml",
        (char*)"../../../../tests/cmocka/network/signalgroup.yaml",
        (char*)"../../../../tests/cmocka/network/network.yaml",
        (char*)"../../../../tests/cmocka/network/model.yaml",
        (char*)"../../../../tests/cmocka/network/runnable.yaml",
    };
    SimMock* mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    simmock_load(mock);
    simmock_setup(mock, "signal", "network");

    /* Return the mock. */
    *state = mock;
    return 0;
}

static int test_teardown(void** state)
{
    SimMock* mock = *state;
    simmock_exit(mock);
    simmock_free(mock);
    return 0;
}

#define SIG_reset_counters  0
#define SIG_task_init_done  1
#define SIG_task_5_active   2
#define SIG_task_10_active  3
#define SIG_task_20_active  4
#define SIG_task_40_active  5
#define SIG_task_5_counter  6
#define SIG_task_10_counter 7
#define SIG_task_20_counter 8
#define SIG_task_40_counter 9

void test_network__network2target2network(void** state)
{
    SimMock*   mock = *state;
    ModelMock* network_model = &mock->model[1];
    assert_non_null(network_model);

    /* 0-19.5ms */
    {
        for (uint32_t i = 0; i < 39; i++) {
            assert_int_equal(simmock_step(mock, true), 0);
        }
        SignalCheck s_checks[] = {
            { .index = SIG_task_init_done, .value = 1.0 },
            { .index = SIG_task_5_active, .value = 1.0 },
            { .index = SIG_task_5_counter, .value = 3.0 },
            { .index = SIG_task_10_active, .value = 1.0 },
            { .index = SIG_task_10_counter, .value = 1.0 },
            { .index = SIG_task_20_active, .value = 0.0 },
            { .index = SIG_task_20_counter, .value = 0.0 },
        };
        simmock_signal_check(
            mock, "target_inst", s_checks, ARRAY_SIZE(s_checks), NULL);
        simmock_signal_check(
            mock, "network_inst", s_checks, ARRAY_SIZE(s_checks), NULL);
    }
    /* Inject a message carrying the reset_counters signal. */
    uint8_t buffer[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    simmock_write_frame(mock->sv_network_tx, "can_bus", buffer, 8, 0x1f0u);
    assert_int_equal(simmock_step(mock, true), 0);
    /* 20ms */
    {
        for (uint32_t i = 0; i < 1; i++) {
            assert_int_equal(simmock_step(mock, true), 0);
        }
        SignalCheck s_checks[] = {
            { .index = SIG_task_init_done, .value = 1.0 },
            { .index = SIG_task_5_active, .value = 1.0 },
            { .index = SIG_task_5_counter, .value = 1.0 },
            { .index = SIG_task_10_active, .value = 1.0 },
            { .index = SIG_task_10_counter, .value = 1.0 },
            { .index = SIG_task_20_active, .value = 1.0 },
            { .index = SIG_task_20_counter, .value = 1.0 },
        };
        FrameCheck f_checks[] = {
            { .frame_id = 0x1f4u, .offset = 1, .value = 0x02 },
            { .frame_id = 0x1f4u, .offset = 2, .value = 0x02 },
            { .frame_id = 0x1f4u, .offset = 3, .value = 0x02 },
            { .frame_id = 0x1f5u, .offset = 4, .value = 0x02 },
            { .frame_id = 0x1f7u, .offset = 4, .value = 0x02 },
        };
        simmock_print_scalar_signals(mock, LOG_DEBUG);
        simmock_print_network_frames(mock, LOG_DEBUG);
        assert_int_equal(network_model->sv_network->length[0] > 0, true);
        simmock_signal_check(
            mock, "network_inst", s_checks, ARRAY_SIZE(s_checks), NULL);
        simmock_frame_check(
            mock, "network_inst", "can_bus", f_checks, ARRAY_SIZE(f_checks));
    }
}

int run_network_tests(void)
{
    void*                   s = test_setup;
    void*                   t = test_teardown;
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_network__network2target2network, s, t),
    };
    return cmocka_run_group_tests_name("NETWORK", tests, NULL, NULL);
}
