// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/logger.h>
#include <dse/testing.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <mock.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


extern uint8_t __log_level__;


static int _sv_nop(ModelDesc* model, double* model_time, double stop_time)
{
    UNUSED(model);
    UNUSED(model_time);
    UNUSED(stop_time);
    return 0;
}


static int test_setup(void** state)
{
    static char* argv[] = {
        (char*)"test_map_index",
        (char*)"--name=map_index",
        (char*)"resources/simbus/map_index.yaml",
    };

    /* Create and return the mock. */
    ModelCMock* m = calloc(1, sizeof(ModelCMock));
    m->argv = argv;
    m->argc = ARRAY_SIZE(argv);
    m->model_name = "Map";
    m->vtable = (ModelVTable){ .step = _sv_nop };
    mock_setup(m);
    *state = m;
    return 0;
}


static int test_teardown(void** state)
{
    ModelCMock* m = *state;
    if (m) {
        mock_teardown(m);
        free(m);
    }
    return 0;
}


typedef struct TC_dim {
    const char* v;
    const char* s;
    uint32_t    vi;
    double      value;
} TC_dim;
void test_simbus__map_index(void** state)
{
    ModelCMock* m = *state;
    UNUSED(m);

    //__log_level__ = LOG_DEBUG;

    TC_dim tc[] = {
        { .v = "one", .s = "a", .vi = 0, .value = 1 },
        { .v = "one", .s = "b", .vi = 1, .value = 2 },
        { .v = "one", .s = "c", .vi = 2, .value = 3 },
        { .v = "two", .s = "d", .vi = 2, .value = 4 },
        { .v = "two", .s = "e", .vi = 0, .value = 5 },
        { .v = "two", .s = "f", .vi = 1, .value = 6 },
        { .v = "three", .s = "g", .vi = 0, .value = 7 },
        { .v = "three", .s = "h", .vi = 2, .value = 8 },
        { .v = "three", .s = "i", .vi = 1, .value = 9 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        log_info("TC[%u]: v=%s, s=%s, vi=%u, value=%f", i, tc[i].v, tc[i].s,
            tc[i].vi, tc[i].value);
        SimbusVectorIndex idx = simbus_vector_lookup(&m->sim, tc[i].v, tc[i].s);
        assert_non_null(idx.sbv);
        assert_non_null(idx.sbv->scalar);
        assert_int_equal(idx.vi, tc[i].vi);
    }
}


int run_map_index_tests(void)
{
#define TGNAME "SIMBUS / MAP INDEX"
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_simbus__map_index, s, t),
    };

    return cmocka_run_group_tests_name(TGNAME, tests, NULL, NULL);
}
