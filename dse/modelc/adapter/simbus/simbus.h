// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_
#define DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_


#include <stdint.h>
#include <stdbool.h>
#include <dse/platform.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/runtime.h>

typedef struct SimbusScalarModelRef {
    void*    mfc;
    uint32_t local_index;
} SimbusScalarModelRef;

typedef struct SimbusVector {
    // map{signal:uint32_t} -> index to vectors
    HashMap index;

    uint32_t  count;
    char**    signal;
    uint32_t* uid;
    double*   scalar;
    uint32_t* changed_indices;
    uint32_t  changed_count;
    uint32_t  changed_capacity;
    uint32_t* changed_generation;
    uint32_t  generation;
    bool      dense_changes;
    Vector*   scalar_models;
    void**    binary;
    uint32_t* length;
    uint32_t* buffer_size;
} SimbusVector;


typedef struct SimbusChannel {
    const char*  name;
    bool         is_binary;
    SimpleSet    signals;
    SimbusVector vector;
} SimbusChannel;


/* adapter.c */
DLL_PUBLIC Adapter* simbus_adapter_create(
    Endpoint* endpoint, double bus_step_size);
DLL_PUBLIC void simbus_adapter_init_channel(
    AdapterModel* am, const char* channel_name, uint32_t expected_model_count);
DLL_PUBLIC void simbus_adapter_run(Adapter* adapter);


#endif  // DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_
