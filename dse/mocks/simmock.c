// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <dse/testing.h>
#include <dse/clib/util/yaml.h>
#include <dse/ncodec/codec.h>
#include <dse/mocks/simmock.h>


/* Private interface from ncodec.c */
extern void* model_sv_stream_create(SignalVector* sv, uint32_t idx);
extern void  model_sv_stream_destroy(void* stream);

/* Private interface from dse/modelc/controller/step.c */
extern int step_model(ModelInstanceSpec* mi, double* model_time);

/* Internal functions. */
static void __set_ncodec_node_id(NCODEC* nc, char* node_id)
{
    assert_non_null(nc);
    ncodec_config(nc, (struct NCodecConfigItem){
                          .name = "node_id",
                          .value = node_id,
                      });
}

static char* __get_ncodec_node_id(NCODEC* nc)
{
    assert_non_null(nc);
    int   index = 0;
    char* node_id = NULL;
    while (index >= 0) {
        NCodecConfigItem ci = ncodec_stat(nc, &index);
        if (strcmp(ci.name, "node_id") == 0) {
            if (ci.value) node_id = strdup(ci.value);
            return node_id;
        }
        index++;
    }
    return node_id;
}


/**
simmock_alloc
=============

Create a SimMock object.

Parameters
----------
inst_names[] (const char*)
: Array of model instance names which should be allocated in the SimMock
  object.

count (size_t)
: The number of elements in the `inst_names` array.

Returns
-------
SimMock*
: The allocated SimMock object. Caller should free by calling `simmock_free()`.
*/
void* simmock_alloc(const char* inst_names[], size_t count)
{
    SimMock* mock = calloc(1, sizeof(SimMock));
    mock->model = calloc(count + 1, sizeof(ModelMock));
    for (size_t i = 0; i < count; i++) {
        mock->model[i].name = inst_names[i];
    }
    mock->step_size = 0.0005;
    return mock;
}


static void __free_stub_sv(SignalVector* sv)
{
    if (sv == NULL) return;

    for (uint32_t i = 0; i < sv->count; i++) {
        if (sv->binary[i]) free(sv->binary[i]);
        NCodecInstance* nc = sv->ncodec[i];
        if (nc) {
            if (nc->stream) model_sv_stream_destroy(nc->stream);
            ncodec_close((NCODEC*)nc);
        }
    }
    if (sv->signal) free(sv->signal);
    if (sv->binary) free(sv->binary);
    if (sv->length) free(sv->length);
    if (sv->buffer_size) free(sv->buffer_size);
    if (sv->mime_type) free(sv->mime_type);
    if (sv->ncodec) free(sv->ncodec);

    free(sv);
}

/**
simmock_free
============

Destroy and free resources allocated to a SimMock object.

Parameters
----------
mock (SimMock*)
: The SimMock object to be released.
*/
void simmock_free(SimMock* mock)
{
    if (mock == NULL) return;

    if (mock->model) free(mock->model);
    if (mock->sv_signal) {
        if (mock->sv_signal->scalar) free(mock->sv_signal->scalar);
        free(mock->sv_signal);
    }
    __free_stub_sv(mock->sv_network_rx);
    __free_stub_sv(mock->sv_network_tx);
    if (mock->doc_list) dse_yaml_destroy_doc_list(mock->doc_list);

    free(mock);
}


/**
simmock_configure
=================

Configure a SimMock object with a list of command line arguments (as would be
used by the ModelC.exe).

Example
-------

{{< readfile file="../examples/simmock_configure.c" code="true" lang="c" >}}

Parameters
----------
mock (SimMock*)
: A SimMock object.

argv (char*)
: Array of arguments.

argc (size_t)
: Number of elements in the `argv` array.

expect_model_count (size_t)
: The expected number of models to be configured (based on the parsed
list of arguments).
*/
void simmock_configure(
    SimMock* mock, char** argv, size_t argc, size_t expect_model_count)
{
    assert_non_null(mock);

    ModelCArguments args;
    modelc_set_default_args(&args, "NOP", mock->step_size, 0.1);
    modelc_parse_arguments(&args, argc, argv, "SimMock");
    int rc = modelc_configure(&args, &mock->sim);
    assert_int_equal(rc, 0);

    for (size_t i = 0; i < expect_model_count; i++) {
        assert_string_equal(
            mock->model[i].name, mock->sim.instance_list[i].name);
    }
}


/**
simmock_load
============

Load all of the models referenced by a SimMock object.

Parameters
----------
mock (SimMock*)
: A SimMock object.

*/
void simmock_load(SimMock* mock)
{
    assert_non_null(mock);

    for (ModelMock* model = mock->model; model->name; model++) {
        log_debug("Load model: %s", model->name);
        model->mi = modelc_get_model_instance(&mock->sim, model->name);
        assert_non_null(model->mi);

        void* handle = dlopen(
            model->mi->model_definition.full_path, RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL) log_notice("ERROR: dlopen call: %s", dlerror());
        assert_non_null(handle);
        model->vtable.create = dlsym(handle, MODEL_CREATE_FUNC_NAME);
        model->vtable.step = dlsym(handle, MODEL_STEP_FUNC_NAME);
        model->vtable.destroy = dlsym(handle, MODEL_DESTROY_FUNC_NAME);
    }

    /* Save a doc_list reference for simmock_free(). */
    mock->doc_list = mock->model->mi->yaml_doc_list;
}


/**
simmock_load_model_check
========================

Check the condition/state of a loaded model.

Parameters
----------
mock (SimMock*)
: A SimMock object.

expect_create_func (bool)
: Indicate that model libraries should contain a `model_create` function.

expect_step_func (bool)
: Indicate that model libraries should contain a `model_step` function.

expect_destroy_func (bool)
: Indicate that model libraries should contain a `model_destroy` function.
*/
void simmock_load_model_check(ModelMock* model, bool expect_create_func,
    bool expect_step_func, bool expect_destroy_func)
{
    assert_non_null(model);

    log_debug("Load model check: %s", model->name);
    assert_non_null(model->mi);

    if (expect_create_func) assert_non_null(model->vtable.create);
    if (expect_step_func) assert_non_null(model->vtable.step);
    if (expect_destroy_func) assert_non_null(model->vtable.destroy);
}


static SignalVector* __stub_sv(SignalVector* sv)
{
    if (sv == NULL) return NULL;
    SignalVector* stub_sv = calloc(1, sizeof(SignalVector));

    stub_sv->mi = sv->mi;
    stub_sv->count = sv->count;
    stub_sv->is_binary = sv->is_binary;
    stub_sv->annotation = sv->annotation;
    stub_sv->append = sv->append;
    stub_sv->release = sv->release;
    stub_sv->reset = sv->reset;
    stub_sv->codec = sv->codec;

    stub_sv->signal = calloc(stub_sv->count, sizeof(const char*));
    stub_sv->binary = calloc(stub_sv->count, sizeof(void*));
    stub_sv->length = calloc(stub_sv->count, sizeof(uint32_t));
    stub_sv->buffer_size = calloc(stub_sv->count, sizeof(uint32_t));
    stub_sv->mime_type = calloc(stub_sv->count, sizeof(const char*));
    stub_sv->ncodec = calloc(stub_sv->count, sizeof(NCODEC*));

    for (uint32_t i = 0; i < stub_sv->count; i++) {
        stub_sv->signal[i] = sv->signal[i];
        stub_sv->mime_type[i] = sv->mime_type[i];
        void*   stream = model_sv_stream_create(stub_sv, i);
        NCODEC* nc = ncodec_open(stub_sv->mime_type[i], stream);
        if (nc) {
            stub_sv->ncodec[i] = nc;
        } else {
            model_sv_stream_destroy(stream);
        }
    }

    return stub_sv;
}

/**
simmock_setup
=============

Calls the `model_setup()` function on each model and then creates Signal Vectors
for each model. Additional Signal Vectors are created to facilitate the
mocked behaviour of a simulation (via the SimMock object).

Parameters
----------
mock (SimMock*)
: A SimMock object.

sig_name (const char*)
: The name of the scalar channel.

sig_name (const char*)
: The name of the binary channel.
*/
void simmock_setup(SimMock* mock, const char* sig_name, const char* net_name)
{
    assert_non_null(mock);
    SignalVector* sv = NULL;

    for (ModelMock* model = mock->model; model->name; model++) {
        log_debug("Setup the Model: %s", model->name);
        log_debug("Call modelc_model_create(): %s", model->name);
        int rc = modelc_model_create(&mock->sim, model->mi, &model->vtable);
        assert_int_equal(rc, 0);

        assert_non_null(model->mi->model_desc);
        assert_ptr_equal(&mock->sim, model->mi->model_desc->sim);
        assert_ptr_equal(model->mi, model->mi->model_desc->mi);
        assert_ptr_equal(
            model->vtable.create, model->mi->model_desc->vtable.create);
        assert_ptr_equal(
            model->vtable.step, model->mi->model_desc->vtable.step);
        assert_ptr_equal(
            model->vtable.destroy, model->mi->model_desc->vtable.destroy);
        assert_non_null(model->mi->model_desc->sv);
        model->sv_save = sv = model->mi->model_desc->sv;

        /* Locate the vectors ... */
        while (sv && sv->name) {
            if (sig_name && (strcmp(sv->name, sig_name) == 0))
                model->sv_signal = sv;
            if (net_name && (strcmp(sv->name, net_name) == 0))
                model->sv_network = sv;
            /* Next signal vector. */
            sv++;
        }
    }

    /* Setup the mocked signal exchange. */
    if (mock->model->sv_signal) {
        mock->sv_signal = calloc(1, sizeof(SignalVector));
        mock->sv_signal->count = mock->model->sv_signal->count;
        mock->sv_signal->scalar =
            calloc(mock->sv_signal->count, sizeof(double));
        memcpy(mock->sv_signal->scalar, mock->model->sv_signal->scalar,
            (mock->sv_signal->count * sizeof(double)));
    }
    if (mock->model->sv_network) {
        mock->sv_network_rx = __stub_sv(mock->model->sv_network);
        mock->sv_network_tx = __stub_sv(mock->model->sv_network);
    }
}


/**
simmock_step
============

Calls `model_step()` on each model and manages the mocked exchange of both
scalar and binary Signal Vectors.

Parameters
----------
mock (SimMock*)
: A SimMock object.

assert_rc (bool)
: Indicate that an assert check (value 0) should be made for each return
  from a call to `model_step()`.

Returns
-------
int
: The combined (or'ed) return code of each call to `model_step()`.
*/
int simmock_step(SimMock* mock, bool assert_rc)
{
    assert_non_null(mock);

    /* Copy simmock->binary_tx to simmock->binary_rx */
    if (mock->sv_network_rx && mock->sv_network_tx) {
        for (uint32_t i = 0; i < mock->sv_network_tx->count; i++) {
            mock->sv_network_rx->reset(mock->sv_network_rx, i);
            mock->sv_network_rx->append(mock->sv_network_rx, i,
                mock->sv_network_tx->binary[i], mock->sv_network_tx->length[i]);
            mock->sv_network_tx->reset(mock->sv_network_tx, i);
        }
    }

    int rc = 0;
    for (ModelMock* model = mock->model; model->name; model++) {
        /* Copy scalars from simmock->scalars. */
        for (uint32_t i = 0; i < mock->sv_signal->count; i++) {
            model->sv_signal->scalar[i] = mock->sv_signal->scalar[i];
        }
        /* Copy binary from simmock->binary_rx. */
        if (mock->sv_network_rx && mock->sv_network_tx) {
            for (uint32_t i = 0; i < mock->sv_network_tx->count; i++) {
                model->sv_network->reset(model->sv_network, i);
                model->sv_network->append(model->sv_network, i,
                    mock->sv_network_rx->binary[i],
                    mock->sv_network_rx->length[i]);
            }
        }
        rc |= modelc_step(model->mi, mock->step_size);
        if (mock->sv_network_rx && mock->sv_network_tx) {
            for (uint32_t i = 0; i < model->sv_network->count; i++) {
                mock->sv_network_tx->append(mock->sv_network_tx, i,
                    model->sv_network->binary[i], model->sv_network->length[i]);
            }
        }
        /* Copy scalars to simmock->scalars. */
        for (uint32_t i = 0; i < mock->sv_signal->count; i++) {
            mock->sv_signal->scalar[i] = model->sv_signal->scalar[i];
        }
        /* Copy binary to simmock->binary_tx. */
        if (mock->sv_network_rx && mock->sv_network_tx) {
            for (uint32_t i = 0; i < mock->sv_network_tx->count; i++) {
                mock->sv_network_tx->append(mock->sv_network_tx, i,
                    model->sv_network->binary[i], model->sv_network->length[i]);
            }
        }
        if (assert_rc) assert_int_equal(rc, 0);
    }
    mock->model_time += mock->step_size;
    return rc;
}


/**
simmock_exit
============

Call `model_exit()` for each model.

Parameters
----------
mock (SimMock*)
: A SimMock object.
*/
void simmock_exit(SimMock* mock)
{
    assert_non_null(mock);
    modelc_exit(&mock->sim);
}


/**
simmock_find_model
==================

Find a MockModel object contained within a SimMock object.

Parameters
----------
mock (SimMock*)
: A SimMock object.

name (const char*)
: The name of the model to find.

Returns
-------
MockModel*
: The identified MockModel object.

NULL
: No MockModel was found.
*/
ModelMock* simmock_find_model(SimMock* mock, const char* name)
{
    for (ModelMock* model = mock->model; model->name; model++) {
        if (strcmp(model->name, name) == 0) return model;
    }
    return NULL;
}


/**
simmock_signal_check
====================

Check the values of various signals on the Signal Vector (scalar).

Example
-------

{{< readfile file="../examples/simmock_signal_check.c" code="true" lang="c" >}}

Parameters
----------
mock (SimMock*)
: A SimMock object.

model_name (const char*)
: The name of the model to check.

checks (SignalCheck*)
: Array of SignalCheck objects.

count (size_t)
: The number elements in the checks array.

func (SignalCheckFunc)
: Optional function pointer for performing the signal check.
*/
void simmock_signal_check(SimMock* mock, const char* model_name,
    SignalCheck* checks, size_t count, SignalCheckFunc func)
{
    assert_non_null(mock);
    ModelMock* check_model = simmock_find_model(mock, model_name);
    assert_non_null(check_model);
    for (size_t i = 0; i < count; i++) {
        if (func) {
            int rc = func(&checks[i], check_model->sv_signal);
            assert_int_equal(rc, 0);
        } else {
            assert_true(checks[i].index < check_model->sv_signal->count);
            assert_double_equal(check_model->sv_signal->scalar[checks[i].index],
                checks[i].value, 0.0);
        }
    }
}


/**
simmock_binary_check
====================

Check the content of a binary signal.

Parameters
----------
mock (SimMock*)
: A SimMock object.

model_name (const char*)
: The name of the model to check.

checks (BinaryCheck*)
: Array of BinaryCheck objects.

count (size_t)
: The number elements in the checks array.

func (BinaryCheckFunc)
: Optional function pointer for performing the binary check.
*/
void simmock_binary_check(SimMock* mock, const char* model_name,
    BinaryCheck* checks, size_t count, BinaryCheckFunc func)
{
    assert_non_null(mock);
    ModelMock* check_model = simmock_find_model(mock, model_name);
    assert_non_null(check_model);

    SignalVector* sv = check_model->sv_network;
    for (size_t i = 0; i < count; i++) {
        if (func) {
            int rc = func(&checks[i], sv);
            assert_int_equal(rc, 0);
        } else {
            assert_true(checks[i].index < sv->count);
            assert_int_equal(sv->length[checks[i].index], checks[i].len);
            if (sv->binary[checks[i].index]) {
                assert_memory_equal(sv->binary[checks[i].index],
                    checks[i].buffer, checks[i].len);
            }
        }
    }
}


/**
simmock_frame_check
===================

Check the content of a binary signal for various frames. The binary signal
should be represented by a Network Codec (NCodec) object.

Example
-------

{{< readfile file="../examples/simmock_frame_check.c" code="true" lang="c" >}}

Parameters
----------
mock (SimMock*)
: A SimMock object.

model_name (const char*)
: The name of the model to check.

sig_name (const char*)
: The name of the binary signal where frames should be located.

checks (FrameCheck*)
: Array of FrameCheck objects.

count (size_t)
: The number elements in the checks array.
*/
void simmock_frame_check(SimMock* mock, const char* model_name,
    const char* sig_name, FrameCheck* checks, size_t count)
{
    assert_non_null(mock);
    ModelMock* model = simmock_find_model(mock, model_name);
    assert_non_null(model);

    SignalVector* sv = model->sv_network;
    NCODEC*       nc = NULL;
    for (uint32_t idx = 0; idx < sv->count; idx++) {
        if (strcmp(sv->signal[idx], sig_name) == 0) {
            nc = sv->codec(sv, idx);
            break;
        }
    }
    assert_non_null(nc);

    NCodecInstance* _nc = (NCodecInstance*)nc;
    char*           node_id_save = __get_ncodec_node_id(nc);
    __set_ncodec_node_id(nc, (char*)"42");

    for (size_t i = 0; i < count; i++) {
        FrameCheck check = checks[i];
        bool       frame_found = false;

        _nc->stream->seek(nc, 0, NCODEC_SEEK_SET);
        NCodecMessage msg = {};
        while (ncodec_read(nc, &msg) != -ENOMSG) {
            if (msg.len == 0) continue;
            if (msg.frame_id != check.frame_id) continue;

            frame_found = true;
            if (check.not_present) assert_false(frame_found);

            assert_true(check.offset < msg.len);
            assert_int_equal(check.value, msg.buffer[check.offset]);
            break;
        }
        if (frame_found == false && check.not_present == false) {
            log_error("check frame %04x not found", check.frame_id);
        }
        if (check.not_present == false) assert_true(frame_found);
    }

    __set_ncodec_node_id(nc, node_id_save);
    free(node_id_save);
    _nc->stream->seek(nc, 0, NCODEC_SEEK_SET);
}


/**
simmock_write_frame
===================

Write a frame, using the associated NCodec object, to the specified
binary signal.

Parameters
----------
sv (SignalVector*)
: A Signal Vector object.

sig_name (const char*)
: The name of the binary signal where frames should be written.

data (uint8_t*)
: Array of data to write (in the frame).

len (size_t)
: Length of the data array.

frame_id (uint32_t)
: The Frame ID associated with the data.
*/
void simmock_write_frame(SignalVector* sv, const char* sig_name, uint8_t* data,
    size_t len, uint32_t frame_id)
{
    assert_non_null(sv);
    assert_true(sv->is_binary);
    NCODEC* nc = NULL;
    for (uint32_t idx = 0; idx < sv->count; idx++) {
        if (strcmp(sv->signal[idx], sig_name) == 0) {
            nc = sv->codec(sv, idx);
            break;
        }
    }
    assert_non_null(nc);

    /* Write a frame with modified node_id, so the frame is not filtered. */
    char* node_id_save = __get_ncodec_node_id(nc);
    __set_ncodec_node_id(nc, (char*)"42");
    ncodec_write(nc, &(struct NCodecMessage){
                         .frame_id = frame_id, .len = len, .buffer = data });
    ncodec_flush(nc);
    __set_ncodec_node_id(nc, node_id_save);
    free(node_id_save);
}

/**
simmock_read_frame
==================

Read a frame, using the associated NCodec object, from the specified
binary signal.

Parameters
----------
sv (SignalVector*)
: A Signal Vector object.

sig_name (const char*)
: The name of the binary signal where frames should be written.

data (uint8_t*)
: Array for the read data. The data is preallocated by the caller.

len (size_t)
: Length of the data array.

Returns
-------
uint32_t
: The Frame ID associated with the read data.

0
: No frame was found in the specified binary signal.
*/
uint32_t simmock_read_frame(
    SignalVector* sv, const char* sig_name, uint8_t* data, size_t len)
{
    assert_non_null(sv);
    assert_true(sv->is_binary);
    NCODEC* nc = NULL;
    for (uint32_t idx = 0; idx < sv->count; idx++) {
        if (strcmp(sv->signal[idx], sig_name) == 0) {
            nc = sv->codec(sv, idx);
            break;
        }
    }
    assert_non_null(nc);
    NCodecInstance* _nc = (NCodecInstance*)nc;
    char*           node_id_save = __get_ncodec_node_id(nc);
    NCodecMessage   msg = {};
    int             rlen = 0;

    /* Read a frame with modified node_id, so the frame is not filtered. */
    __set_ncodec_node_id(nc, (char*)"42");
    _nc->stream->seek(nc, 0, NCODEC_SEEK_SET);
    rlen = ncodec_read(nc, &msg);
    __set_ncodec_node_id(nc, node_id_save);
    free(node_id_save);

    /* Process the message. */
    if (rlen > 0) {
        uint32_t length = msg.len;
        if (length > len) length = len;
        memcpy(data, msg.buffer, length);
        return msg.frame_id;
    } else {
        for (size_t i = 0; i < len; i++)
            data[i] = 0x55; /* Mark the return buffer with junk. */
        return 0;
    }
}


/**
simmock_print_scalar_signals
============================

Print the scalar signal values of each signal vector of each model.

Parameters
----------
mock (SimMock*)
: A SimMock object.

level (int)
: The log level to print at.
*/
void simmock_print_scalar_signals(SimMock* mock, int level)
{
    assert_non_null(mock);

    log_at(level, "SignalVector (Scalar) at %f:", mock->model_time);
    for (ModelMock* model = mock->model; model->name; model++) {
        log_at(level, " Model Instance: %s", model->name);
        for (uint32_t i = 0; i < model->sv_signal->count; i++) {
            log_at(level, "  [%d] %s = %f", i, model->sv_signal->signal[i],
                model->sv_signal->scalar[i]);
        }
    }
}


/**
simmock_print_binary_signals
============================

Print the binary signals contained in each network vector of each model.

Parameters
----------
mock (SimMock*)
: A SimMock object.

level (int)
: The log level to print at.
*/
void simmock_print_binary_signals(SimMock* mock, int level)
{
    assert_non_null(mock);

    log_at(level, "SignalVector (Network) at %f:", mock->model_time);
    for (ModelMock* model = mock->model; model->name; model++) {
        log_at(level, " Model Instance: %s", model->name);
        SignalVector* sv = model->sv_network;
        for (uint32_t idx = 0; idx < sv->count; idx++) {
            log_at(level, "  Network Signal: %s (%d)", sv->signal[idx],
                sv->length[idx]);
            uint8_t* b = sv->binary[idx];
            for (uint32_t i = 0; i < sv->length[idx]; i += 8) {
                char buffer[100] = {};
                for (uint32_t j = i; j < i + 8; j++) {
                    if (j < sv->length[idx]) {
                        snprintf(buffer + strlen(buffer),
                            sizeof(buffer + strlen(buffer)), "%02x ", b[j]);
                    }
                }
                log_at(level, "%s", buffer);
            }
        }
    }
}


/**
simmock_print_network_frames
============================

Print the frames contained in each network vector of each model.

Parameters
----------
mock (SimMock*)
: A SimMock object.

level (int)
: The log level to print at.
*/
void simmock_print_network_frames(SimMock* mock, int level)
{
    assert_non_null(mock);

    log_at(level, "SignalVector (Network) at %f:", mock->model_time);
    for (ModelMock* model = mock->model; model->name; model++) {
        log_at(level, " Model Instance: %s", model->name);
        SignalVector* sv = model->sv_network;
        for (uint32_t idx = 0; idx < sv->count; idx++) {
            NCODEC*         nc = sv->codec(sv, idx);
            NCodecInstance* _nc = (NCodecInstance*)nc;
            if (nc == NULL) continue;
            char* node_id_save = __get_ncodec_node_id(nc);

            if (0) {
                uint8_t* b = sv->binary[idx];
                for (uint32_t i = 0; i < sv->length[idx]; i += 8)
                    printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
                        b[i + 0], b[i + 1], b[i + 2], b[i + 3], b[i + 4],
                        b[i + 5], b[i + 6], b[i + 7]);
            }

            log_at(level, "  Network Signal: %s (%d)", sv->signal[idx],
                sv->length[idx]);
            __set_ncodec_node_id(nc, (char*)"42");
            _nc->stream->seek(nc, 0, NCODEC_SEEK_SET);
            NCodecMessage msg = {};
            while (ncodec_read(nc, &msg) != -ENOMSG) {
                if (msg.len == 0) continue;
                log_at(level, "  Frame:  %04x", msg.frame_id);
                char buffer[100] = {};
                for (uint32_t i = 0; i < msg.len; i++) {
                    snprintf(
                        buffer + strlen(buffer), 10, "%02x ", msg.buffer[i]);
                    if ((i % 8 == 7)) {
                        log_at(level, "   %s", buffer);
                        buffer[0] = 0;
                    }
                }
            }
            __set_ncodec_node_id(nc, node_id_save);
            free(node_id_save);
            _nc->stream->seek(nc, 0, NCODEC_SEEK_SET);
        }
    }
}
