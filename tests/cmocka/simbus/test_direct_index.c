// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
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
    ModelCMock* m = calloc(1, sizeof(ModelCMock));
    m->argv = (char*[]){
        (char*)"test_direct_index",
        (char*)"--name=direct_index",
        (char*)"resources/simbus/direct_index.yaml",
    };
    m->argc = 3;
    m->model_name = "Direct";
    m->vtable = (ModelVTable){ .step = _sv_nop };
    mock_setup(m);
    /* Return the mock. */
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


void test_simbus__direct_index(void** state)
{
    ModelCMock* m = *state;
    UNUSED(m);
    SimbusVectorIndex index;

    /* Check the SBV objects have correct references. */
    index = simbus_vector_lookup(&m->sim, "one", NULL);
    assert_non_null(index.sbv);
    assert_non_null(index.sbv->scalar);
    assert_string_equal("a", index.sbv->signal[0]);
    assert_string_equal("b", index.sbv->signal[1]);
    assert_string_equal("c", index.sbv->signal[2]);
    assert_non_null(index.direct_index.map);
    assert_int_equal(0, index.direct_index.offset);
    assert_int_equal(216, index.direct_index.size);

    index = simbus_vector_lookup(&m->sim, "two", NULL);
    assert_non_null(index.sbv);
    assert_non_null(index.sbv->scalar);
    assert_string_equal("d", index.sbv->signal[0]);
    assert_string_equal("e", index.sbv->signal[1]);
    assert_string_equal("f", index.sbv->signal[2]);
    assert_non_null(index.direct_index.map);
    assert_int_equal(72, index.direct_index.offset);
    assert_int_equal(216, index.direct_index.size);

    index = simbus_vector_lookup(&m->sim, "three", NULL);
    assert_non_null(index.sbv);
    assert_non_null(index.sbv->scalar);
    assert_string_equal("g", index.sbv->signal[0]);
    assert_string_equal("h", index.sbv->signal[1]);
    assert_string_equal("i", index.sbv->signal[2]);
    assert_non_null(index.direct_index.map);
    assert_int_equal(144, index.direct_index.offset);
    assert_int_equal(216, index.direct_index.size);

    /* Check the SBV objects have equal references. */
    SimbusVectorIndex i1 = simbus_vector_lookup(&m->sim, "one", NULL);
    SimbusVectorIndex i2 = simbus_vector_lookup(&m->sim, "two", NULL);
    SimbusVectorIndex i3 = simbus_vector_lookup(&m->sim, "three", NULL);
    assert_ptr_equal(i1.direct_index.map, i2.direct_index.map);
    assert_ptr_equal(i1.direct_index.map, i3.direct_index.map);

    /* Check that the Map and SBV represent the same memory. */
    index = simbus_vector_lookup(&m->sim, "one", NULL);
    assert_ptr_equal(i1.sbv->scalar, index.direct_index.map);
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            double* s = (double*)(index.direct_index.map + (i * 72));
            s[j] = i * 3 + j + 1;
        }
    }
    assert_double_equal(1, i1.sbv->scalar[0], 0);
    assert_double_equal(2, i1.sbv->scalar[1], 0);
    assert_double_equal(3, i1.sbv->scalar[2], 0);
    assert_double_equal(4, i2.sbv->scalar[0], 0);
    assert_double_equal(5, i2.sbv->scalar[1], 0);
    assert_double_equal(6, i2.sbv->scalar[2], 0);
    assert_double_equal(7, i3.sbv->scalar[0], 0);
    assert_double_equal(8, i3.sbv->scalar[1], 0);
    assert_double_equal(9, i3.sbv->scalar[2], 0);

    /* Check binary variables are also handled. */
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            void**    b = (void**)(index.direct_index.map + (i * 72) + 24);
            uint32_t* b_len =
                (uint32_t*)(index.direct_index.map + (i * 72) + 48);
            uint32_t* b_size =
                (uint32_t*)(index.direct_index.map + (i * 72) + 60);
            uint32_t buf_len = 10 + i * 3 + j + 1;
            char*    buf = calloc(buf_len, sizeof(char));
            snprintf(buf, buf_len, "%lu", i * 3 + j + 1);
            dse_buffer_append(
                &b[j], &b_len[j], &b_size[j], buf, 10 + i * 3 + j + 1);
            free(buf);
        }
    }

    assert_string_equal("1", i1.sbv->binary[0]);
    assert_string_equal("2", i1.sbv->binary[1]);
    assert_string_equal("3", i1.sbv->binary[2]);
    assert_string_equal("4", i2.sbv->binary[0]);
    assert_string_equal("5", i2.sbv->binary[1]);
    assert_string_equal("6", i2.sbv->binary[2]);
    assert_string_equal("7", i3.sbv->binary[0]);
    assert_string_equal("8", i3.sbv->binary[1]);
    assert_string_equal("9", i3.sbv->binary[2]);

    assert_int_equal(11, i1.sbv->length[0]);
    assert_int_equal(12, i1.sbv->length[1]);
    assert_int_equal(13, i1.sbv->length[2]);
    assert_int_equal(14, i2.sbv->length[0]);
    assert_int_equal(15, i2.sbv->length[1]);
    assert_int_equal(16, i2.sbv->length[2]);
    assert_int_equal(17, i3.sbv->length[0]);
    assert_int_equal(18, i3.sbv->length[1]);
    assert_int_equal(19, i3.sbv->length[2]);

    assert_int_equal(11, i1.sbv->buffer_size[0]);
    assert_int_equal(12, i1.sbv->buffer_size[1]);
    assert_int_equal(13, i1.sbv->buffer_size[2]);
    assert_int_equal(14, i2.sbv->buffer_size[0]);
    assert_int_equal(15, i2.sbv->buffer_size[1]);
    assert_int_equal(16, i2.sbv->buffer_size[2]);
    assert_int_equal(17, i3.sbv->buffer_size[0]);
    assert_int_equal(18, i3.sbv->buffer_size[1]);
    assert_int_equal(19, i3.sbv->buffer_size[2]);
}


typedef struct TC_dim {
    const char* v;
    const char* s;
    size_t      offset;
    double      value;
} TC_dim;
void test_simbus__direct_index_map(void** state)
{
    ModelCMock* m = *state;
    UNUSED(m);
    SimbusVectorIndex index;

    TC_dim tc[] = {
        { .v = "one", .s = "a", .offset = 0, .value = 1 },
        { .v = "one", .s = "b", .offset = 8, .value = 2 },
        { .v = "one", .s = "c", .offset = 16, .value = 3 },
        { .v = "two", .s = "d", .offset = 72 + 0, .value = 4 },
        { .v = "two", .s = "e", .offset = 72 + 8, .value = 5 },
        { .v = "two", .s = "f", .offset = 72 + 16, .value = 6 },
        { .v = "three", .s = "g", .offset = 144 + 0, .value = 7 },
        { .v = "three", .s = "h", .offset = 144 + 8, .value = 8 },
        { .v = "three", .s = "i", .offset = 144 + 16, .value = 9 },
    };
    index = simbus_vector_lookup(&m->sim, "one", NULL);
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        SimbusVectorIndex idx = simbus_vector_lookup(&m->sim, tc[i].v, tc[i].s);
        assert_non_null(idx.sbv);
        assert_non_null(idx.sbv->scalar);
        idx.sbv->scalar[idx.vi] = tc[i].value;
        assert_double_equal(
            tc[i].value, *(double*)(index.direct_index.map + tc[i].offset), 0);
    }
}


int run_direct_index_tests(void)
{
#define TGNAME "SIMBUS / DIRECT INDEX"
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_simbus__direct_index, s, t),
        cmocka_unit_test_setup_teardown(test_simbus__direct_index_map, s, t),
    };

    return cmocka_run_group_tests_name(TGNAME, tests, NULL, NULL);
}
