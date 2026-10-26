// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <dse/logger.h>
#include <dse/testing.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/controller/model_private.h>
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
    ModelInstancePrivate* mip = m->mi->private;
    AdapterModelIndexItem model_item = { .uid = m->mi->uid,
        .am = mip->adapter_model };
    vector_push(&m->controller->adapter->models, &model_item);
    vector_sort(&m->controller->adapter->models);
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


static void test_simbus__legacy_scalar_sync(void** state)
{
    ModelCMock*           m = *state;
    ModelInstancePrivate* mip = m->mi->private;
    AdapterModel*         am = mip->adapter_model;
    ChannelIndexItem*     channel_item = vector_find(
        &am->channels, &(ChannelIndexItem){ .name = "one" }, 0, NULL);
    assert_non_null(channel_item);
    Channel*              channel = channel_item->ch;
    ModelFunctionChannel* mfc = channel->mfc;
    assert_non_null(mfc);
    model_function_channel_free_flat_sync(mfc);
    mfc->scalar_sync.capable = false;

    SimbusVectorIndex index = simbus_vector_lookup(&m->sim, "one", "a");
    for (uint32_t step = 1; step <= 3; step++) {
        double value = step * 10.0;
        index.sbv->scalar[index.vi] = value;
        assert_int_equal(adapter_model_start(am->adapter, &m->sim), 0);
        marshal_model(m->mi, MARSHAL_ADAPTER2MODEL_SCALAR_ONLY);
        for (uint32_t signal = 0; signal < mfc->signal_count; signal++) {
            if (strcmp(mfc->signal_names[signal], "a") != 0) continue;
            assert_double_equal(mfc->signal_value_double[signal], value, 0);
        }
        for (uint32_t signal = 0; signal < channel->index.count; signal++) {
            SignalValue* sv = channel->index.map[signal].signal;
            if (strcmp(sv->name, "a") != 0) continue;
            assert_double_equal(sv->val, value, 0);
            assert_double_equal(sv->final_val, value, 0);
        }
    }
}


static void test_simbus__legacy_publish_to_flat_scalar(void** state)
{
    ModelCMock*           m = *state;
    ModelInstancePrivate* mip = m->mi->private;
    AdapterModel*         am = mip->adapter_model;
    ChannelIndexItem*     channel_item = vector_find(
        &am->channels, &(ChannelIndexItem){ .name = "one" }, 0, NULL);
    assert_non_null(channel_item);
    Channel*              channel = channel_item->ch;
    ModelFunctionChannel* mfc = channel->mfc;
    assert_non_null(mfc);

    assert_int_equal(adapter_ready(am->adapter, &m->sim), 0);
    assert_true(mfc->scalar_sync.enabled);
    for (uint32_t step = 1; step <= 3; step++) {
        double value = step * 10.0;
        mfc->scalar_sync.enabled = false;
        for (uint32_t signal = 0; signal < channel->index.count; signal++) {
            SignalValue* sv = channel->index.map[signal].signal;
            if (strcmp(sv->name, "a") != 0) continue;
            sv->final_val = value;
        }
        assert_int_equal(adapter_model_ready(am->adapter, &m->sim), 0);
        mfc->scalar_sync.enabled = true;
        assert_int_equal(adapter_model_start(am->adapter, &m->sim), 0);
        for (uint32_t signal = 0; signal < mfc->signal_count; signal++) {
            if (strcmp(mfc->signal_names[signal], "a") != 0) continue;
            assert_double_equal(mfc->signal_value_double[signal], value, 0);
            assert_double_equal(
                mfc->scalar_sync.output_shadow[signal], value, 0);
        }
    }
}


static void test_simbus__transformed_scalar_sync(void** state)
{
    ModelCMock*           m = *state;
    ModelInstancePrivate* mip = m->mi->private;
    AdapterModel*         am = mip->adapter_model;
    Channel*              channel = adapter_get_channel(am, "one");
    assert_non_null(channel);
    ModelFunctionChannel* mfc = channel->mfc;
    assert_non_null(mfc);
    assert_true(mfc->scalar_sync.capable);
    assert_null(mfc->signal_transform);

    uint32_t local_index = mfc->signal_count;
    for (uint32_t signal = 0; signal < mfc->signal_count; signal++) {
        if (strcmp(mfc->signal_names[signal], "a") == 0) {
            local_index = signal;
            break;
        }
    }
    assert_true(local_index < mfc->signal_count);
    SignalTransform* transforms =
        calloc(mfc->signal_count, sizeof(*transforms));
    assert_non_null(transforms);
    transforms[local_index].linear.factor = 2.0;
    transforms[local_index].linear.offset = 10.0;
    model_function_channel_free_flat_sync(mfc);
    mfc->signal_transform = transforms;

    assert_int_equal(adapter_ready(am->adapter, &m->sim), 0);
    assert_false(mfc->scalar_sync.enabled);
    assert_null(mfc->scalar_sync.simbus_indices);
    assert_null(mfc->scalar_sync.output_shadow);
    SimbusVectorIndex index = simbus_vector_lookup(&m->sim, "one", "a");
    assert_non_null(index.sbv);
    for (uint32_t step = 1; step <= 3; step++) {
        double bus_input = step * 10.0;
        index.sbv->scalar[index.vi] = bus_input;
        assert_int_equal(adapter_model_start(am->adapter, &m->sim), 0);
        marshal_model(m->mi, MARSHAL_ADAPTER2MODEL_SCALAR_ONLY);
        assert_double_equal(
            mfc->signal_value_double[local_index], bus_input * 2.0 + 10.0, 0);

        double bus_output = bus_input + 5.0;
        mfc->signal_value_double[local_index] = bus_output * 2.0 + 10.0;
        marshal_model(m->mi, MARSHAL_MODEL2ADAPTER_SCALAR_ONLY);
        assert_int_equal(adapter_model_ready(am->adapter, &m->sim), 0);
        assert_double_equal(index.sbv->scalar[index.vi], bus_output, 0);
        assert_true(index.sbv->changed_count > 0);
        assert_false(mfc->scalar_sync.enabled);

        assert_int_equal(adapter_model_start(am->adapter, &m->sim), 0);
        marshal_model(m->mi, MARSHAL_ADAPTER2MODEL_SCALAR_ONLY);
        assert_double_equal(
            mfc->signal_value_double[local_index], bus_output * 2.0 + 10.0, 0);
    }

    mfc->signal_transform = NULL;
    free(transforms);
}


static void check_scalar_propagation_modes(void** state, bool legacy_publisher)
{
    ModelCMock*           m = *state;
    ModelInstancePrivate* mip = m->mi->private;
    AdapterModel*         publisher = mip->adapter_model;
    Adapter*              adapter = publisher->adapter;
    enum { SIGNAL_COUNT = 100 };
    char        signal_names[SIGNAL_COUNT][16];
    const char* publisher_names[SIGNAL_COUNT];
    const char* subscriber_names[SIGNAL_COUNT];
    for (uint32_t signal = 0; signal < SIGNAL_COUNT; signal++) {
        snprintf(signal_names[signal], sizeof(signal_names[signal]),
            "signal_%03u", signal);
        publisher_names[signal] = signal_names[signal];
        subscriber_names[signal] = signal_names[SIGNAL_COUNT - signal - 1];
    }
    double               publisher_values[SIGNAL_COUNT] = { 0 };
    double               subscriber_values[SIGNAL_COUNT] = { 0 };
    double               expected[SIGNAL_COUNT] = { 0 };
    ModelFunctionChannel publisher_mfc = {
        .channel_name = "propagation",
        .signal_names = publisher_names,
        .signal_count = ARRAY_SIZE(publisher_names),
        .signal_value_double = publisher_values,
        .scalar_sync = { .capable = !legacy_publisher,
            .enabled = !legacy_publisher },
    };
    ModelFunctionChannel subscriber_mfc = {
        .channel_name = "propagation",
        .signal_names = subscriber_names,
        .signal_count = ARRAY_SIZE(subscriber_names),
        .signal_value_double = subscriber_values,
        .scalar_sync = { .capable = true, .enabled = true },
    };
    Channel* publisher_channel = adapter_init_channel(publisher, "propagation",
        publisher_names, ARRAY_SIZE(publisher_names), &publisher_mfc);
    assert_non_null(publisher_channel);
    AdapterModel* subscriber = calloc(1, sizeof(*subscriber));
    assert_non_null(subscriber);
    subscriber->adapter = adapter;
    subscriber->model_uid = publisher->model_uid + 1;
    subscriber->channels =
        vector_make(sizeof(ChannelIndexItem), 0, adapter_name2ch_compar);
    assert_non_null(adapter_init_channel(subscriber, "propagation",
        subscriber_names, ARRAY_SIZE(subscriber_names), &subscriber_mfc));
    AdapterModelIndexItem subscriber_item = { .uid = subscriber->model_uid,
        .am = subscriber };
    vector_push(&adapter->models, &subscriber_item);
    vector_sort(&adapter->models);
    assert_int_equal(adapter->vtable->register_(publisher), 0);
    assert_int_equal(adapter->vtable->register_(subscriber), 0);
    assert_int_equal(adapter_ready(adapter, &m->sim), 0);
    assert_int_equal(publisher_mfc.scalar_sync.enabled, !legacy_publisher);
    assert_true(subscriber_mfc.scalar_sync.enabled);

    SimbusVectorIndex index =
        simbus_vector_lookup(&m->sim, "propagation", NULL);
    assert_non_null(index.sbv);
    assert_int_equal(index.sbv->count, ARRAY_SIZE(expected));
    const struct {
        uint32_t changed_count;
        bool     dense;
    } cases[] = {
        { 0, false },
        { 1, false },
        { 5, false },
        { 25, false },
        { 49, false },
        { 50, true },
        { 75, true },
        { 100, true },
        { 1, false },
        { 0, false },
    };
    for (uint32_t step = 0; step < ARRAY_SIZE(cases); step++) {
        uint32_t changed_count = cases[step].changed_count;
        log_debug("%s publisher: %u/%u changed, expected %s\n",
            legacy_publisher ? "legacy" : "flat-map", changed_count,
            SIGNAL_COUNT,
            cases[step].dense ? "dense"
                              : (changed_count ? "sparse" : "no propagation"));
        for (uint32_t signal = 0; signal < changed_count; signal++) {
            double value = (step + 1) * 100.0 + signal;
            expected[signal] = value;
            if (legacy_publisher) {
                SignalMap* signal_map = adapter_get_signal_map(
                    publisher, "propagation", &publisher_names[signal], 1);
                assert_non_null(signal_map);
                signal_map[0].signal->final_val = value;
                free(signal_map);
            } else {
                publisher_values[signal] = value;
            }
        }

        uint32_t generation = index.sbv->generation;
        assert_int_equal(adapter_model_ready(adapter, &m->sim), 0);
        assert_int_equal(index.sbv->dense_changes, cases[step].dense);
        if (cases[step].dense) {
            assert_true(index.sbv->changed_count >= index.sbv->count / 2);
        } else {
            assert_int_equal(index.sbv->changed_count, changed_count);
        }
        for (uint32_t signal = 0; signal < ARRAY_SIZE(expected); signal++) {
            SimbusVectorIndex signal_index = simbus_vector_lookup(
                &m->sim, "propagation", publisher_names[signal]);
            assert_double_equal(
                index.sbv->scalar[signal_index.vi], expected[signal], 0);
        }

        assert_int_equal(adapter_model_start(adapter, &m->sim), 0);
        for (uint32_t signal = 0; signal < ARRAY_SIZE(expected); signal++) {
            uint32_t publisher_signal = ARRAY_SIZE(expected) - signal - 1;
            assert_double_equal(
                subscriber_values[signal], expected[publisher_signal], 0);
            assert_double_equal(
                subscriber_mfc.scalar_sync.output_shadow[signal],
                expected[publisher_signal], 0);
        }
        assert_int_equal(index.sbv->changed_count, 0);
        assert_false(index.sbv->dense_changes);
        assert_int_equal(index.sbv->generation, generation + 1);
    }

    AdapterModelIndexItem publisher_item = { .uid = publisher->model_uid,
        .am = publisher };
    vector_reset(&adapter->models);
    vector_push(&adapter->models, &publisher_item);
    adapter_destroy_adapter_model(subscriber);
    publisher_channel->mfc = NULL;
    model_function_channel_free_flat_sync(&publisher_mfc);
    model_function_channel_free_flat_sync(&subscriber_mfc);
}


static void test_simbus__flat_scalar_propagation_modes(void** state)
{
    check_scalar_propagation_modes(state, false);
}


static void test_simbus__legacy_scalar_propagation_modes(void** state)
{
    check_scalar_propagation_modes(state, true);
}


int run_map_index_tests(void)
{
#define TGNAME "SIMBUS / MAP INDEX"
    void* s = test_setup;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_simbus__map_index, s, t),
        cmocka_unit_test_setup_teardown(test_simbus__legacy_scalar_sync, s, t),
        cmocka_unit_test_setup_teardown(
            test_simbus__legacy_publish_to_flat_scalar, s, t),
        cmocka_unit_test_setup_teardown(
            test_simbus__transformed_scalar_sync, s, t),
        cmocka_unit_test_setup_teardown(
            test_simbus__flat_scalar_propagation_modes, s, t),
        cmocka_unit_test_setup_teardown(
            test_simbus__legacy_scalar_propagation_modes, s, t),
    };

    return cmocka_run_group_tests_name(TGNAME, tests, NULL, NULL);
}
