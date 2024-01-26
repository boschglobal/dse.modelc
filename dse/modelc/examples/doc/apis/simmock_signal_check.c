#include <dse/testing.h>
#include <dse/mocks/simmock.h>

#define SIG_task_init_done  1
#define SIG_task_5_active   2
#define SIG_task_5_counter  6

void test_network__signal_check(void** state)
{
    SimMock* mock = *state;

    /* 0ms - initial tick, Rx consumed, Tx of initial content. */
    {
        for (uint32_t i = 0; i < 1; i++) {
            assert_int_equal(simmock_step(mock, true), 0);
        }
        SignalCheck s_checks[] = {
            { .index = SIG_task_init_done, .value = 1.0 },
            { .index = SIG_task_5_active, .value = 0.0 },
            { .index = SIG_task_5_counter, .value = 0.0 },
        };
        simmock_print_scalar_signals(mock, LOG_DEBUG);
        simmock_signal_check(mock, "network_inst", s_checks, ARRAY_SIZE(s_checks), NULL);
    }
}
