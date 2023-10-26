// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>


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


static int _sv_nop(double* model_time, double stop_time)
{
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
    rc = model_function_register(mock->mi, "NOP", 0.005, _sv_nop);
    assert_int_equal(rc, 0);

    /* Scalar channel. */
    static ModelChannelDesc scalar_channel_desc = {
        .name = "scalar_vector",
        .function_name = "NOP",
    };
    rc = model_configure_channel(mock->mi, &scalar_channel_desc);
    assert_int_equal(rc, 0);

    /* Binary channel. */
    static ModelChannelDesc binary_channel_desc = {
        .name = "binary_vector",
        .function_name = "NOP",
    };
    rc = model_configure_channel(mock->mi, &binary_channel_desc);
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

    SignalVector* sv_save = model_sv_create(mock->mi);
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
    assert_string_equal(sv->function_name, "NOP");
    assert_int_equal(sv->count, 2);
    assert_int_equal(sv->is_binary, false);
    assert_non_null(sv->signal);
    assert_non_null(sv->scalar);
    assert_non_null(sv->append);
    assert_non_null(sv->reset);
    assert_non_null(sv->release);
    assert_non_null(sv->annotation);

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
        sv->reset(sv, i);                    /* NOP */
        sv->release(sv, i);                  /* NOP */
        sv->append(sv, i, (void*)"1234", 4); /* NOP */
        assert_double_equal(sv->scalar[i], test_val, DBL_EPSILON);
    }

    model_sv_destroy(sv_save);
}


void test_signal__binary(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = model_sv_create(mock->mi);
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
    assert_string_equal(sv->function_name, "NOP");
    assert_int_equal(sv->count, 2);
    assert_int_equal(sv->is_binary, true);
    assert_non_null(sv->signal);
    assert_non_null(sv->mime_type);
    assert_non_null(sv->binary);
    assert_non_null(sv->length);
    assert_non_null(sv->buffer_size);
    assert_non_null(sv->append);
    assert_non_null(sv->reset);
    assert_non_null(sv->release);
    assert_non_null(sv->annotation);

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
        /* Append to the value. */
        sv->append(sv, i, test_val, test_val_len);
        assert_non_null(sv->binary[i]);
        assert_string_equal((char*)sv->binary[i], test_val);
        assert_int_equal(sv->length[i], test_val_len);
        assert_int_equal(sv->buffer_size[i], test_val_len);
        /* Reset the value. */
        sv->reset(sv, i);
        assert_non_null(sv->binary[i]);
        assert_int_equal(sv->length[i], 0);
        assert_int_equal(sv->buffer_size[i], test_val_len);
        /* Append to the value with embedded NULL. */
        test_val[5] = '\0';
        sv->append(sv, i, test_val, test_val_len);
        assert_non_null(sv->binary[i]);
        assert_int_equal(sv->length[i], test_val_len);
        assert_int_equal(sv->buffer_size[i], test_val_len);
        /* Release the value. */
        sv->release(sv, i);
        assert_null(sv->binary[i]);
        assert_int_equal(sv->length[i], 0);
        assert_int_equal(sv->buffer_size[i], 0);
        /* Cleanup. */
        free(test_val);
    }

    model_sv_destroy(sv_save);
}


void test_signal__annotations(void** state)
{
    ModelCMock* mock = *state;

    SignalVector* sv_save = model_sv_create(mock->mi);
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

    value = sv->annotation(sv, 0, "name");
    assert_string_equal(value, "binary_foo");
    value = sv->annotation(sv, 0, "mime_type");
    assert_null(value);

    value = sv->annotation(sv, 1, "name");
    assert_string_equal(value, "binary_bar");
    value = sv->annotation(sv, 1, "mime_type");
    assert_string_equal(value, "application/custom-stream");

    model_sv_destroy(sv_save);
}


int run_signal_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_signal__scalar, s, t),
        cmocka_unit_test_setup_teardown(test_signal__binary, s, t),
        cmocka_unit_test_setup_teardown(test_signal__annotations, s, t),
    };

    return cmocka_run_group_tests_name("NCODEC", tests, NULL, NULL);
}
