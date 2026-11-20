// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/modelc/runtime.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>

#define TARGET_INST                    "target_inst"
#define VM_INST_NAME                   "__virtual_model__"
#define RUNTIME_TEST_SIM_PATH          "resources/runtime"
#define RUNTIME_BINARY_TARGET_MESSAGE  "target-message"
#define RUNTIME_BINARY_VIRTUAL_MESSAGE "virtual-message"
#define RUNTIME_NCODEC_TARGET_ECU_ID   "1"
#define RUNTIME_NCODEC_TARGET_SWC_ID   "2"
#define RUNTIME_NCODEC_VIRTUAL_ECU_ID  "3"
#define RUNTIME_NCODEC_VIRTUAL_SWC_ID  "4"

extern uint8_t __log_level__;

static bool     virtual_binary_sent;
static bool     virtual_binary_received_both;
static bool     virtual_ncodec_sent;
static uint32_t virtual_ncodec_received_mask;
static uint32_t virtual_ncodec_received_count;


static int test_setup_scalar(void** state)
{
    RuntimeModelDesc* rm = calloc(1, sizeof(RuntimeModelDesc));
    assert_non_null(rm);
    *rm = (RuntimeModelDesc){
        .model = {
            .sim = calloc(1, sizeof(SimulationSpec)),
        },
        .runtime = {
            .runtime_model = "tester",
            .sim_path = strdup(RUNTIME_TEST_SIM_PATH),
            .simulation_yaml = "scalar/simulation.yaml",
            .model_name = TARGET_INST,
            .virtual_model_name = VM_INST_NAME,
            .step_size = 0.0005,
            .end_time = 0.0100,
            .log_level = __log_level__,
        },
    };
    assert_non_null(rm->model.sim);
    *state = rm;
    return 0;
}


static int test_setup_binary(void** state)
{
    RuntimeModelDesc* rm = calloc(1, sizeof(RuntimeModelDesc));
    assert_non_null(rm);
    *rm = (RuntimeModelDesc){
        .model = {
            .sim = calloc(1, sizeof(SimulationSpec)),
        },
        .runtime = {
            .runtime_model = "tester",
            .sim_path = strdup(RUNTIME_TEST_SIM_PATH),
            .simulation_yaml = "binary/simulation.yaml",
            .model_name = TARGET_INST,
            .virtual_model_name = VM_INST_NAME,
            .step_size = 0.001,
            .end_time = 0.003,
            .log_level = __log_level__,
        },
    };
    assert_non_null(rm->model.sim);
    virtual_binary_sent = false;
    virtual_binary_received_both = false;
    *state = rm;
    return 0;
}


static int test_setup_ncodec(void** state)
{
    RuntimeModelDesc* rm = calloc(1, sizeof(RuntimeModelDesc));
    assert_non_null(rm);
    *rm = (RuntimeModelDesc){
        .model = {
            .sim = calloc(1, sizeof(SimulationSpec)),
        },
        .runtime = {
            .runtime_model = "tester",
            .sim_path = strdup(RUNTIME_TEST_SIM_PATH),
            .simulation_yaml = "ncodec/simulation.yaml",
            .model_name = TARGET_INST,
            .virtual_model_name = VM_INST_NAME,
            .step_size = 0.001,
            .end_time = 0.003,
            .log_level = __log_level__,
        },
    };
    assert_non_null(rm->model.sim);
    virtual_ncodec_sent = false;
    virtual_ncodec_received_mask = 0;
    virtual_ncodec_received_count = 0;
    *state = rm;
    return 0;
}


static int test_teardown(void** state)
{
    RuntimeModelDesc* rm = *state;
    if (rm) {
        if (rm->runtime.doc_list) model_runtime_destroy(rm);
        free(rm->runtime.sim_path);
        free(rm->model.sim);
        free(rm);
    }
    return 0;
}


static ModelSignalIndex _signal(ModelInstanceSpec* mi, const char* name)
{
    return signal_index(mi->model_desc, "scalar_vector", name);
}


static void _assert_scalar_signals(ModelInstanceSpec* mi)
{
    SignalVector* sv = mi->model_desc->sv;
    assert_non_null(sv);
    assert_string_equal(sv[0].alias, "scalar_vector");
    assert_int_equal(sv[0].count, 2);
    assert_string_equal(sv[0].signal[0], "counter");
    assert_string_equal(sv[0].signal[1], "check");
}


static SignalVector* _find_signal_vector(ModelDesc* model, const char* alias)
{
    for (SignalVector* sv = model->sv; sv && sv->name; sv++) {
        if (sv->alias && strcmp(sv->alias, alias) == 0) return sv;
    }
    return NULL;
}


static void _assert_binary_signal(ModelDesc* model)
{
    SignalVector* sv = _find_signal_vector(model, "binary_vector");
    assert_non_null(sv);
    assert_true(sv->is_binary);
    assert_int_equal(sv->count, 1);
    assert_string_equal(sv->signal[0], "message");
}


static bool _runtime_binary_has_both(const uint8_t* buffer, size_t buffer_len)
{
    const size_t target_len = sizeof(RUNTIME_BINARY_TARGET_MESSAGE);
    const size_t virtual_len = sizeof(RUNTIME_BINARY_VIRTUAL_MESSAGE);
    if (buffer == NULL || buffer_len != target_len + virtual_len) return false;

    return (memcmp(buffer, RUNTIME_BINARY_TARGET_MESSAGE, target_len) == 0 &&
               memcmp(buffer + target_len, RUNTIME_BINARY_VIRTUAL_MESSAGE,
                   virtual_len) == 0) ||
           (memcmp(buffer, RUNTIME_BINARY_VIRTUAL_MESSAGE, virtual_len) == 0 &&
               memcmp(buffer + virtual_len, RUNTIME_BINARY_TARGET_MESSAGE,
                   target_len) == 0);
}


static int _runtime_virtual_step(
    ModelDesc* model, double* model_time, double stop_time)
{
    ModelSignalIndex counter = signal_index(model, "scalar_vector", "counter");
    ModelSignalIndex check = signal_index(model, "scalar_vector", "check");
    if (counter.scalar == NULL || check.scalar == NULL) return -EINVAL;

    *counter.scalar += 10.0;
    *check.scalar = *counter.scalar;
    *model_time = stop_time;
    return 0;
}


static int _runtime_virtual_binary_step(
    ModelDesc* model, double* model_time, double stop_time)
{
    ModelSignalIndex message = signal_index(model, "binary_vector", "message");
    if (message.sv == NULL) return -EINVAL;

    uint8_t* buffer = NULL;
    size_t   buffer_len = 0;
    signal_read(message.sv, message.signal, &buffer, &buffer_len);
    virtual_binary_received_both = _runtime_binary_has_both(buffer, buffer_len);
    signal_reset(message.sv, message.signal);
    if (!virtual_binary_sent) {
        signal_append(message.sv, message.signal,
            (void*)RUNTIME_BINARY_VIRTUAL_MESSAGE,
            sizeof(RUNTIME_BINARY_VIRTUAL_MESSAGE));
        virtual_binary_sent = true;
    }

    *model_time = stop_time;
    return 0;
}


static int _runtime_virtual_ncodec_step(
    ModelDesc* model, double* model_time, double stop_time)
{
    ModelSignalIndex pdu = signal_index(model, "ncodec_vector", "pdu");
    if (pdu.binary == NULL) return -EINVAL;
    NCODEC* ncodec = signal_codec(pdu.sv, pdu.signal);
    if (ncodec == NULL) return -EINVAL;

    NCodecPdu received = {};
    int       len;
    while ((len = ncodec_read(ncodec, &received)) >= 0) {
        (void)len;
        assert_int_equal(received.id, 1001);
        assert_int_equal(received.swc_id, 2);
        virtual_ncodec_received_mask |= 1U;
        virtual_ncodec_received_count++;
    }

    if (!virtual_ncodec_sent) {
        static const uint8_t payload[] = "virtual-pdu";
        ncodec_truncate(ncodec);
        ncodec_write(ncodec, &(NCodecPdu){
                                 .id = 2001,
                                 .payload = (uint8_t*)payload,
                                 .payload_len = sizeof(payload),
                                 .ecu_id = 3,
                                 .swc_id = 4,
                             });
        ncodec_flush(ncodec);
        virtual_ncodec_sent = true;
    }

    *model_time = stop_time;
    return 0;
}


static void _assert_ncodec_signal(
    ModelDesc* model, const char* ecu_id, const char* swc_id)
{
    SignalVector* sv = _find_signal_vector(model, "ncodec_vector");
    assert_non_null(sv);
    assert_true(sv->is_binary);
    assert_int_equal(sv->count, 1);
    assert_string_equal(sv->signal[0], "pdu");
    NCODEC* ncodec = signal_codec(sv, 0);
    assert_non_null(ncodec);

    bool             ecu_id_found = false;
    bool             swc_id_found = false;
    NCodecConfigItem item;
    for (int32_t index = 0; index >= 0; index++) {
        item = ncodec_stat(ncodec, &index);
        if (item.name == NULL) break;
        if (strcmp(item.name, "ecu_id") == 0) {
            assert_string_equal(item.value, ecu_id);
            ecu_id_found = true;
        } else if (strcmp(item.name, "swc_id") == 0) {
            assert_string_equal(item.value, swc_id);
            swc_id_found = true;
        }
        if (ecu_id_found && swc_id_found) break;
    }
    assert_true(ecu_id_found);
    assert_true(swc_id_found);
}


static void test_runtime_scalar_create(void** state)
{
    RuntimeModelDesc* rm = *state;

    assert_ptr_equal(model_runtime_create(rm), rm);
    assert_non_null(rm->model.sim);

    ModelInstanceSpec* model =
        modelc_get_model_instance(rm->model.sim, TARGET_INST);
    assert_non_null(model);
    assert_non_null(model->name);
    assert_string_equal(model->name, TARGET_INST);
    assert_false(model->virtual_model);
    assert_non_null(model->model_desc);
    _assert_scalar_signals(model);

    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(rm->model.sim, VM_INST_NAME);
    assert_non_null(virtual_model);
    assert_non_null(virtual_model->name);
    assert_string_equal(virtual_model->name, VM_INST_NAME);
    assert_true(virtual_model->virtual_model);
    assert_non_null(virtual_model->model_desc);
    assert_non_null(virtual_model->model_desc->sv);
    _assert_scalar_signals(virtual_model);
}


static void test_runtime_binary_create(void** state)
{
    RuntimeModelDesc* rm = *state;
    assert_ptr_equal(model_runtime_create(rm), rm);

    ModelInstanceSpec* target =
        modelc_get_model_instance(rm->model.sim, TARGET_INST);
    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(rm->model.sim, VM_INST_NAME);
    assert_non_null(target);
    assert_non_null(virtual_model);

    SignalVector* target_scalar =
        _find_signal_vector(target->model_desc, "scalar_vector");
    assert_non_null(target_scalar);
    assert_false(target_scalar->is_binary);
    assert_int_equal(target_scalar->count, 2);
    assert_string_equal(target_scalar->signal[0], "target_received");
    assert_string_equal(target_scalar->signal[1], "virtual_received");
    _assert_binary_signal(target->model_desc);
    _assert_binary_signal(virtual_model->model_desc);
}


static void test_runtime_binary_sim(void** state)
{
    RuntimeModelDesc* rm = *state;
    assert_ptr_equal(model_runtime_create(rm), rm);

    ModelInstanceSpec* target =
        modelc_get_model_instance(rm->model.sim, TARGET_INST);
    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(rm->model.sim, VM_INST_NAME);
    assert_non_null(target);
    assert_non_null(virtual_model);

    ModelSignalIndex target_received = _signal(target, "target_received");
    ModelSignalIndex target_message =
        signal_index(target->model_desc, "binary_vector", "message");
    ModelSignalIndex virtual_message =
        signal_index(virtual_model->model_desc, "binary_vector", "message");
    assert_non_null(target_received.scalar);
    assert_non_null(target_message.binary);
    assert_non_null(virtual_message.binary);
    assert_double_equal(*target_received.scalar, 0.0, 1e-12);
    assert_false(virtual_binary_received_both);

    virtual_model->model_desc->vtable.step = _runtime_virtual_binary_step;

    double model_time = 0.0;
    for (uint32_t step = 1; step <= 2; step++) {
        const double stop_time = step * rm->model.sim->step_size;
        assert_int_equal(model_runtime_step(rm, &model_time, stop_time), 0);
        assert_double_equal(model_time, stop_time, 1e-12);
    }

    assert_double_equal(*target_received.scalar, 1.0, 1e-12);
    assert_true(virtual_binary_received_both);
}


static void test_runtime_ncodec_create(void** state)
{
    RuntimeModelDesc* rm = *state;
    assert_ptr_equal(model_runtime_create(rm), rm);

    ModelInstanceSpec* target =
        modelc_get_model_instance(rm->model.sim, TARGET_INST);
    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(rm->model.sim, VM_INST_NAME);
    assert_non_null(target);
    assert_non_null(virtual_model);
    _assert_ncodec_signal(target->model_desc, RUNTIME_NCODEC_TARGET_ECU_ID,
        RUNTIME_NCODEC_TARGET_SWC_ID);
    _assert_ncodec_signal(virtual_model->model_desc,
        RUNTIME_NCODEC_VIRTUAL_ECU_ID, RUNTIME_NCODEC_VIRTUAL_SWC_ID);
}


static void test_runtime_ncodec_sim(void** state)
{
    RuntimeModelDesc* rm = *state;
    assert_ptr_equal(model_runtime_create(rm), rm);

    ModelInstanceSpec* target =
        modelc_get_model_instance(rm->model.sim, TARGET_INST);
    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(rm->model.sim, VM_INST_NAME);
    assert_non_null(target);
    assert_non_null(virtual_model);

    ModelSignalIndex target_mask =
        signal_index(target->model_desc, "scalar_vector", "received_mask");
    ModelSignalIndex target_count =
        signal_index(target->model_desc, "scalar_vector", "received_count");
    assert_non_null(target_mask.scalar);
    assert_non_null(target_count.scalar);
    assert_double_equal(*target_mask.scalar, 0.0, 1e-12);
    assert_double_equal(*target_count.scalar, 0.0, 1e-12);

    virtual_model->model_desc->vtable.step = _runtime_virtual_ncodec_step;

    double model_time = 0.0;
    for (uint32_t step = 1; step <= 2; step++) {
        const double stop_time = step * rm->model.sim->step_size;
        assert_int_equal(model_runtime_step(rm, &model_time, stop_time), 0);
        assert_double_equal(model_time, stop_time, 1e-12);
    }

    assert_double_equal(*target_mask.scalar, 2.0, 1e-12);
    assert_double_equal(*target_count.scalar, 1.0, 1e-12);
    assert_int_equal(virtual_ncodec_received_mask, 1);
    assert_int_equal(virtual_ncodec_received_count, 1);
}


static void test_runtime_scalar_sim(void** state)
{
    RuntimeModelDesc* rm = *state;

    assert_ptr_equal(model_runtime_create(rm), rm);
    ModelInstanceSpec* target =
        modelc_get_model_instance(rm->model.sim, TARGET_INST);
    ModelInstanceSpec* virtual_model =
        modelc_get_model_instance(rm->model.sim, VM_INST_NAME);
    assert_non_null(target);
    assert_non_null(virtual_model);

    ModelSignalIndex target_counter = _signal(target, "counter");
    ModelSignalIndex target_check = _signal(target, "check");
    ModelSignalIndex virtual_counter = _signal(virtual_model, "counter");
    ModelSignalIndex virtual_check = _signal(virtual_model, "check");
    assert_non_null(target_counter.scalar);
    assert_non_null(target_check.scalar);
    assert_non_null(virtual_counter.scalar);
    assert_non_null(virtual_check.scalar);

    assert_double_equal(*target_counter.scalar, 0.0, 1e-12);
    assert_double_equal(*target_check.scalar, 0.0, 1e-12);
    assert_double_equal(*virtual_counter.scalar, 0.0, 1e-12);
    assert_double_equal(*virtual_check.scalar, 0.0, 1e-12);

    virtual_model->model_desc->vtable.step = _runtime_virtual_step;

    double       model_time = 0.0;
    const double step_size = rm->model.sim->step_size;
    for (uint32_t step = 1; step <= 20; step++) {
        const double stop_time = step * step_size;
        const int    rc = model_runtime_step(rm, &model_time, stop_time);
        assert_int_equal(rc, step == 20 ? 1 : 0);
        assert_double_equal(model_time, stop_time, 1e-12);

        const double previous_runtime_counter = (step - 1) * 10.0;
        assert_double_equal(
            *target_counter.scalar, previous_runtime_counter + 1.0, 1e-12);
        assert_double_equal(
            *target_check.scalar, previous_runtime_counter, 1e-12);
        assert_double_equal(*virtual_counter.scalar, step * 10.0, 1e-12);
        assert_double_equal(*virtual_check.scalar, step * 10.0, 1e-12);
    }
}


int run_runtime_tests(void)
{
    void* sb = test_setup_binary;
    void* sn = test_setup_ncodec;
    void* s = test_setup_scalar;
    void* t = test_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_runtime_scalar_create, s, t),
        cmocka_unit_test_setup_teardown(test_runtime_scalar_sim, s, t),
        cmocka_unit_test_setup_teardown(test_runtime_binary_create, sb, t),
        cmocka_unit_test_setup_teardown(test_runtime_binary_sim, sb, t),
        cmocka_unit_test_setup_teardown(test_runtime_ncodec_create, sn, t),
        cmocka_unit_test_setup_teardown(test_runtime_ncodec_sim, sn, t),
    };
    return cmocka_run_group_tests_name("RUNTIME", tests, NULL, NULL);
}
