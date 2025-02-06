// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MOCKS_SIMMOCK_H_
#define DSE_MOCKS_SIMMOCK_H_

#include <stdbool.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define log_at(level, ...)                                                     \
    do {                                                                       \
        if (level == LOG_ERROR) log_error(__VA_ARGS__);                        \
        if (level == LOG_SIMBUS) log_simbus(__VA_ARGS__);                      \
        if (level == LOG_INFO) log_info(__VA_ARGS__);                          \
        if (level == LOG_DEBUG) log_debug(__VA_ARGS__);                        \
        if (level == LOG_TRACE) log_trace(__VA_ARGS__);                        \
    } while (0)


/**
SimMock API
===========

The SimMock API provides interfaces for the development of CMocka Tests with
a representative mocked implementation of the ModelC <->  SimBus interface.
The mock supports a Signal Vector (scalar) and Network Vector (binary).
Several models may be loaded into the SimMock.


Component Diagram
-----------------
<div hidden>

```
@startuml simmock-objects

title SimMock Objects

package "SimBus Mock" {
    class Signal <<SignalVector>> {
        +uint32_t count
        +double scalar[]
        +annotation()
    }

    class Network <<SignalVector>> {
        +uint32_t count
        +bool is_binary
        +void* binary[]
        #NCODEC* ncodec[]
        +annotation()
        +append()
        +reset()
        #codec()
    }

    file svSig [
    signalgroup.yaml
    ....
    kind: <b>SignalGroup
    metadata:
    name: <b>signal
    labels:
        channel: <b>signal_vector
    ]

    file svNet [
    signalgroup.yaml
    ....
    kind: <b>SignalGroup
    metadata:
    name: <b>network
    labels:
        channel: <b>network_vector
    annotations:
        vector_type: <b>binary
    ]

    Signal -> svSig
    Network -> svNet
}

map ModelMock {
    sv_signal *--> Signal
    sv_network *--> Network
}

map SimMock {
    model *--> ModelMock

    sv_signal *--> Signal
    sv_network_rx *-> Network
    sv_network_tx *-> Network
}

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](simmock-objects.png)

*/

typedef struct ModelMock {
    const char*           name;
    ModelInstanceSpec*    mi;
    SignalVector*         sv_signal;
    SignalVector*         sv_network;
    SignalVector*         sv_save;
    ModelVTable           vtable;
    SignalMap*            sm_signal;
    ModelFunctionChannel* mfc_signal;
} ModelMock;

typedef struct SimMock {
    SimulationSpec sim;
    double         step_size;
    ModelMock*     model;
    void*          doc_list;

    double model_time; /* Indirect, logging only. */

    SignalVector* sv_signal;
    SignalVector* sv_network_rx;
    SignalVector* sv_network_tx;
} SimMock;


/* Simulation API. */
void* simmock_alloc(const char* inst_names[], size_t count);
void  simmock_configure(
     SimMock* mock, char** argv, size_t argc, size_t expect_model_count);
void simmock_load(SimMock* mock);
void simmock_load_model_check(ModelMock* model, bool expect_create_func,
    bool expect_step_func, bool expect_destroy_func);
void simmock_setup(SimMock* mock, const char* sig_name, const char* net_name);
int  simmock_step(SimMock* mock, bool assert_rc);
void simmock_exit(SimMock* mock, bool call_destroy);
ModelMock* simmock_find_model(SimMock* mock, const char* name);
void simmock_write_frame(SignalVector* sv, const char* sig_name, uint8_t* data,
    size_t len, uint32_t frame_id, uint8_t frame_type);
uint32_t simmock_read_frame(
    SignalVector* sv, const char* sig_name, uint8_t* data, size_t len);
void simmock_write_pdu(SignalVector* sv, const char* sig_name, uint8_t* data,
    size_t len, uint32_t id);
uint32_t simmock_read_pdu(
    SignalVector* sv, const char* sig_name, uint8_t* data, size_t len);
void simmock_free(SimMock* mock);


/* Check API. */
typedef struct SignalCheck {
    uint32_t index;
    double   value;
} SignalCheck;

typedef struct BinaryCheck {
    uint32_t index;
    uint8_t* buffer;
    uint32_t len;
} BinaryCheck;

typedef struct FrameCheck {
    uint32_t frame_id;
    uint8_t  offset;
    uint8_t  value;
    bool     not_present;
} FrameCheck;

typedef int (*SignalCheckFunc)(SignalCheck* check, SignalVector* sv);
typedef int (*BinaryCheckFunc)(BinaryCheck* check, SignalVector* sv);

void simmock_signal_check(SimMock* mock, const char* model_name,
    SignalCheck* checks, size_t count, SignalCheckFunc func);
void simmock_binary_check(SimMock* mock, const char* model_name,
    BinaryCheck* checks, size_t count, BinaryCheckFunc func);
void simmock_frame_check(SimMock* mock, const char* model_name,
    const char* sig_name, FrameCheck* checks, size_t count);


/* Information API. */
void simmock_print_scalar_signals(SimMock* mock, int level);
void simmock_print_binary_signals(SimMock* mock, int level);
void simmock_print_network_frames(SimMock* mock, int level);


#endif  // DSE_MOCKS_SIMMOCK_H_
