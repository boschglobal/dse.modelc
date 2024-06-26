// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))


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


void test_signal__scalar(void** state)
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
    assert_non_null(sv->vtable.append);
    assert_non_null(sv->vtable.reset);
    assert_non_null(sv->vtable.release);
    assert_non_null(sv->vtable.annotation);

    /* Signal properties. */
    const char* expected_names[] = {
        "scalar_foo",
        "scalar_bar",
    };
    for (uint32_t i = 0; i < sv->count; i++) {
        double test_val = i * 10.0 + 5;
        assert_string_equal(sv->signal[i], expected_names[i]);
        assert_double_equal(sv->scalar[i], 0.0, DBL_EPSILON);
        /* Toggle the value. */
        sv->scalar[i] = test_val;
        signal_reset(sv, i);                    /* NOP */
        signal_release(sv, i);                  /* NOP */
        signal_append(sv, i, (void*)"1234", 4); /* NOP */
        assert_double_equal(sv->scalar[i], test_val, DBL_EPSILON);
    }
}


void test_signal__binary(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 2);

    /* Use the "binary" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* General properties. */
    assert_string_equal(sv->name, "binary");
    assert_string_equal(sv->alias, "binary_vector");
    assert_string_equal(sv->function_name, "model_step");
    assert_int_equal(sv->count, 2);
    assert_int_equal(sv->is_binary, true);
    assert_non_null(sv->signal);
    assert_non_null(sv->mime_type);
    assert_non_null(sv->binary);
    assert_non_null(sv->length);
    assert_non_null(sv->buffer_size);
    assert_non_null(sv->reset_called);
    assert_non_null(sv->vtable.append);
    assert_non_null(sv->vtable.reset);
    assert_non_null(sv->vtable.release);
    assert_non_null(sv->vtable.annotation);

    /* Signal properties. */
    const char* expected_names[] = {
        "binary_foo",
        "binary_bar",
    };
    const char* expected_mime_types[] = {
        "application/octet-stream",
        "application/custom-stream",
    };
    for (uint32_t i = 0; i < sv->count; i++) {
        char*    test_val = strdup(sv->signal[i]);
        uint32_t test_val_len = strlen(test_val) + 1;
        assert_string_equal(sv->signal[i], expected_names[i]);
        assert_string_equal(sv->mime_type[i], expected_mime_types[i]);
        /* Check the value. */
        assert_null(sv->binary[i]);
        assert_int_equal(sv->length[i], 0);
        assert_int_equal(sv->buffer_size[i], 0);
        assert_false(sv->reset_called[i]);
        signal_reset(sv, i);
        /* Append to the value. */
        signal_append(sv, i, test_val, test_val_len);
        assert_non_null(sv->binary[i]);
        assert_string_equal((char*)sv->binary[i], test_val);
        assert_int_equal(sv->length[i], test_val_len);
        assert_int_equal(sv->buffer_size[i], test_val_len);
        /* Reset the value. */
        signal_reset(sv, i);
        assert_true(sv->reset_called[i]);
        assert_non_null(sv->binary[i]);
        assert_int_equal(sv->length[i], 0);
        assert_int_equal(sv->buffer_size[i], test_val_len);
        /* Append to the value with embedded NULL. */
        test_val[5] = '\0';
        signal_append(sv, i, test_val, test_val_len);
        assert_non_null(sv->binary[i]);
        assert_int_equal(sv->length[i], test_val_len);
        assert_int_equal(sv->buffer_size[i], test_val_len);
        /* Release the value. */
        signal_release(sv, i);
        assert_null(sv->binary[i]);
        assert_int_equal(sv->length[i], 0);
        assert_int_equal(sv->buffer_size[i], 0);
        /* Cleanup. */
        free(test_val);
    }
}


void test_signal__annotations(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 2);

    /* Use the "binary" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* Check annotations. */
    const char* value;

    value = signal_annotation(sv, 0, "name");
    assert_non_null(value);
    assert_string_equal(value, "binary_foo");
    value = signal_annotation(sv, 0, "mime_type");
    assert_null(value);

    value = signal_annotation(sv, 1, "name");
    assert_non_null(value);
    assert_string_equal(value, "binary_bar");
    value = signal_annotation(sv, 1, "mime_type");
    assert_non_null(value);
    assert_string_equal(value, "application/custom-stream");
}


void test_signal__group_annotations(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 2);

    /* Use the "binary" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }

    /* Check annotations. */
    const char* value;

    value = signal_group_annotation(sv, "vector_name");
    assert_non_null(value);
    assert_string_equal(value, "network_vector");
    value = signal_group_annotation(sv, "missing");
    assert_null(value);
}


void test_signal__binary_echo(void** state)
{
    /* ModelC detects and prevents echo of binary data via reset_called.
     *
     * Logic exists in the controller which can only be tested with real
     * operation via ModelC (no mock). This test covers the Model API
     * aspects.
     */
    ModelCMock* mock = *state;

    SignalVector* sv_save = mock->mi->model_desc->sv;
    assert_int_equal(_sv_count(sv_save), 2);

    /* Use the "binary" signal vector. */
    SignalVector* sv = sv_save;
    while (sv && sv->name) {
        if (strcmp(sv->name, "binary") == 0) break;
        /* Next signal vector. */
        sv++;
    }
    assert_string_equal(sv->name, "binary");
    assert_string_equal(sv->alias, "binary_vector");

    /* Initial conditions. */
    assert_int_equal(sv->length[0], 0);
    assert_int_equal(sv->length[1], 0);
    assert_false(sv->reset_called[0]);
    assert_false(sv->reset_called[1]);
    char*    test_val = strdup(sv->signal[0]);
    uint32_t test_val_len = strlen(test_val) + 1;


    /* Append to one signal. */
    int _loglevel_save = __log_level__;
    __log_level__ = LOG_FATAL;
    signal_append(sv, 0, test_val, test_val_len);
    __log_level__ = _loglevel_save;
    assert_int_equal(sv->length[0], test_val_len);
    assert_int_equal(sv->length[1], 0);
    assert_false(sv->reset_called[0]);
    assert_false(sv->reset_called[1]);

    /* Call reset. */
    signal_reset(sv, 0);
    assert_int_equal(sv->length[0], 0);
    assert_int_equal(sv->length[1], 0);
    assert_true(sv->reset_called[0]);
    assert_false(sv->reset_called[1]);

    /* Append again. */
    signal_append(sv, 0, test_val, test_val_len);
    assert_int_equal(sv->length[0], test_val_len);
    assert_int_equal(sv->length[1], 0);
    assert_true(sv->reset_called[0]);
    assert_false(sv->reset_called[1]);

    /* Cleanup. */
    free(test_val);
}


int run_signal_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_signal__scalar, s, t),
        cmocka_unit_test_setup_teardown(test_signal__binary, s, t),
        cmocka_unit_test_setup_teardown(test_signal__annotations, s, t),
        cmocka_unit_test_setup_teardown(test_signal__group_annotations, s, t),
        cmocka_unit_test_setup_teardown(test_signal__binary_echo, s, t),
    };

    return cmocka_run_group_tests_name("SIGNAL", tests, NULL, NULL);
}
