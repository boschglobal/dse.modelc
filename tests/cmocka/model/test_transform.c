// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/mocks/simmock.h>


#define UNUSED(x) ((void)x)


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


static int test_setup_signal(void** state)
{
    ModelCMock* mock = calloc(1, sizeof(ModelCMock));
    assert_non_null(mock);

    int             rc;
    ModelCArguments args;
    char*           argv[] = {
        (char*)"test_signal",
        (char*)"--name=signal",
        (char*)"resources/model/signal.yaml",
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


static int test_setup_transform(void** state)
{
    ModelCMock* mock = calloc(1, sizeof(ModelCMock));
    assert_non_null(mock);

    int             rc;
    ModelCArguments args;
    char*           argv[] = {
        (char*)"test_transform",
        (char*)"--name=transform",
        (char*)"resources/model/transform.yaml",
    };

    modelc_set_default_args(&args, "test", 0.005, 0.005);
    args.log_level = LOG_QUIET;  // = __log_level__;
    modelc_parse_arguments(&args, ARRAY_SIZE(argv), argv, "Transform");
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


void test_transform__parse_no_transform(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 2);

    /* Use the "scalar" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "scalar") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* General properties. */
    assert_string_equal(sv->name, "scalar");
    assert_string_equal(sv->alias, "scalar_vector");
    assert_string_equal(sv->function_name, "model_step");
    assert_int_equal(sv->count, 2);
    assert_int_equal(sv->is_binary, false);
    assert_non_null(sv->signal);
    assert_non_null(sv->scalar);

    /* Transform table (internal variable). */
    ModelFunction* mf = controller_get_model_function(mock->mi, "model_step");
    assert_non_null(mf);
    ModelFunctionChannel* mfc = hashmap_get(&mf->channels, "scalar");
    assert_non_null(mfc);
    assert_null(mfc->signal_transform);
}


void test_transform__parse(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 1);

    /* Use the "scalar" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "scalar") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* General properties. */
    assert_string_equal(sv->name, "scalar");
    assert_string_equal(sv->alias, "scalar_vector");
    assert_string_equal(sv->function_name, "model_step");
    assert_int_equal(sv->count, 5);
    assert_int_equal(sv->is_binary, false);
    assert_non_null(sv->signal);
    assert_non_null(sv->scalar);

    /* Transform table (internal variable). */
    ModelFunction* mf = controller_get_model_function(mock->mi, "model_step");
    assert_non_null(mf);
    ModelFunctionChannel* mfc = hashmap_get(&mf->channels, "scalar");
    assert_non_null(mfc);
    assert_non_null(mfc->signal_transform);

    /* Check the transform table. */
    double tc[][2] = {
        { 0.0, 0.0 },  // No transform.
        { 1.0, 0.0 }, { 2.0, 10.0 }, { 3.0, -200.0 },
        { 0.0, 100.0 },  // Invalid factor, effectively disabled.
    };
    SignalTransform* st = mfc->signal_transform;
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        log_trace("Index: %d (%f, %f)", i, tc[i][0], tc[i][1]);
        assert_double_equal(st[i].linear.factor, tc[i][0], 0.0);
        assert_double_equal(st[i].linear.offset, tc[i][1], 0.0);
    }
}


static int test_setup_simmmock(void** state)
{
    UNUSED(state);
    return 0;
}

static int test_teardown_simmock(void** state)
{
    SimMock* mock = *state;

    simmock_exit(mock, true);
    simmock_free(mock);

    return 0;
}


#define TRANSFORM_INST_NAME "transform"

typedef struct t_tc {
    uint32_t index;
    double   set;
    double   expect;
} t_tc;


static int mock_model_nop(ModelDesc* m, double* model_time, double stop_time)
{
    UNUSED(m);
    *model_time = stop_time;
    return 0;
}

void test_transform__marshal_to_model(void** state)
{
    const char* inst_names[] = {
        TRANSFORM_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_transform",
        (char*)"--name=" TRANSFORM_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"resources/model/transform.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, TRANSFORM_INST_NAME);
    model->vtable.step = mock_model_nop;
    model->mi = modelc_get_model_instance(&mock->sim, model->name);
    mock->doc_list = mock->model->mi->yaml_doc_list;
    simmock_setup(mock, "scalar", NULL);

    assert_non_null(mock->sv_signal);
    assert_non_null(model->sv_signal);

    // T=0
    t_tc tc_initial[] = {
        { .index = 0, .set = 1, .expect = 1.0 },
        { .index = 1, .set = 1, .expect = 1.0 },
        { .index = 2, .set = 1, .expect = 12.0 },
        { .index = 3, .set = 1, .expect = -197.0 },
        { .index = 4, .set = 1, .expect = 1.0 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(tc_initial); i++) {
        mock->sv_signal->scalar[tc_initial[i].index] = tc_initial[i].set;
    }
    assert_int_equal(simmock_step(mock, true), 0);
    for (size_t i = 0; i < ARRAY_SIZE(tc_initial); i++) {
        log_trace("Index: %d (%f -> %f = %f)", i,
            mock->sv_signal->scalar[tc_initial[i].index],
            model->sv_signal->scalar[tc_initial[i].index],
            tc_initial[i].expect);
        assert_double_equal(model->sv_signal->scalar[tc_initial[i].index],
            tc_initial[i].expect, 0.0);
    }

    // Step (sim -> model)
    t_tc tc_sm[] = {
        { .index = 0, .set = 2, .expect = 2.0 },
        { .index = 1, .set = 3, .expect = 3.0 },
        { .index = 2, .set = 4, .expect = 18.0 },
        { .index = 3, .set = 5, .expect = -185.0 },
        { .index = 4, .set = 6, .expect = 6.0 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(tc_sm); i++) {
        mock->sv_signal->scalar[tc_sm[i].index] = tc_sm[i].set;
    }
    assert_int_equal(simmock_step(mock, true), 0);
    for (size_t i = 0; i < ARRAY_SIZE(tc_sm); i++) {
        log_trace("Index: %d (%f -> %f = %f)", i,
            mock->sv_signal->scalar[tc_sm[i].index],
            model->sv_signal->scalar[tc_sm[i].index], tc_sm[i].expect);
        assert_double_equal(
            model->sv_signal->scalar[tc_sm[i].index], tc_sm[i].expect, 0.0);
    }
}


static int mock_model_marshal(
    ModelDesc* m, double* model_time, double stop_time)
{
    m->sv->scalar[0] = 42;
    m->sv->scalar[1] = 43;
    m->sv->scalar[2] = 10;
    m->sv->scalar[3] = 58;
    m->sv->scalar[4] = 44;

    *model_time = stop_time;
    return 0;
}

void test_transform__marshal_from_model(void** state)
{
    const char* inst_names[] = {
        TRANSFORM_INST_NAME,
    };
    char* argv[] = {
        (char*)"test_transform",
        (char*)"--name=" TRANSFORM_INST_NAME,
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"resources/model/transform.yaml",
    };
    SimMock* mock = *state = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    ModelMock* model = simmock_find_model(mock, TRANSFORM_INST_NAME);
    model->vtable.step = mock_model_marshal;
    model->mi = modelc_get_model_instance(&mock->sim, model->name);
    mock->doc_list = mock->model->mi->yaml_doc_list;
    simmock_setup(mock, "scalar", NULL);

    assert_non_null(mock->sv_signal);
    assert_non_null(model->sv_signal);

    // Step (model -> sim)
    t_tc tc_ms[] = {
        { .index = 0, .expect = 42.0 },
        { .index = 1, .expect = 43.0 },
        { .index = 2, .expect = 0.0 },
        { .index = 3, .expect = 86.0 },
        { .index = 4, .expect = 44.0 },
    };
    assert_int_equal(simmock_step(mock, true), 0);
    for (size_t i = 0; i < ARRAY_SIZE(tc_ms); i++) {
        log_trace("Index: %d (%f -> %f = %f)", i,
            mock->sv_signal->scalar[tc_ms[i].index],
            model->sv_signal->scalar[tc_ms[i].index], tc_ms[i].expect);
        assert_double_equal(
            mock->sv_signal->scalar[tc_ms[i].index], tc_ms[i].expect, 0.0);
    }
}


int run_transform_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_transform__parse_no_transform,
            test_setup_signal, test_teardown),
        cmocka_unit_test_setup_teardown(
            test_transform__parse, test_setup_transform, test_teardown),
        cmocka_unit_test_setup_teardown(test_transform__marshal_to_model,
            test_setup_simmmock, test_teardown_simmock),
        //   cmocka_unit_test_setup_teardown(test_transform__marshal_from_model,
        //   test_setup_simmmock, test_teardown_simmock),
    };

    return cmocka_run_group_tests_name("TRANSFORM", tests, NULL, NULL);
}
