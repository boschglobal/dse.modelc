#include <dse/testing.h>
#include <dse/modelc/mocks/simmock.h>

int test_setup(void** state)
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
    simmock_load(mock, true);
    simmock_setup(mock, "signal", "network");

    /* Return the mock. */
    *state = mock;
    return 0;
}
