#include <dse/testing.h>
#include <dse/modelc/mocks/simmock.h>

void test_network__frame_check(void** state)
{
    SimMock* mock = *state;

    /* 0ms - initial tick, Rx consumed, Tx of initial content. */
    {
        for (uint32_t i = 0; i < 1; i++) {
            assert_int_equal(simmock_step(mock, true), 0);
        }
        FrameCheck f_checks[] = {
            { .frame_id = 0x1f3u, .offset = 0, .value = 0x01 },
            { .frame_id = 0x1f4u, .offset = 1, .value = 0x02 },
            { .frame_id = 0x1f5u, .offset = 4, .value = 0x02 },
        };
        simmock_print_network_frames(mock, LOG_DEBUG);
        simmock_frame_check(mock, "network_inst", "can_bus", f_checks, ARRAY_SIZE(f_checks));
    }
}
