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


typedef struct SimbusVector {
    HashMap   index;  // map{signal:uint32_t} -> index to vectors
    uint32_t  count;
    char**    signal;
    uint32_t* uid;
    double*   scalar;
    void**    binary;
    uint32_t* length;
    uint32_t* buffer_size;
} SimbusVector;


typedef struct SimbusChannel {
    const char*  name;
    SimpleSet    signals;
    SimbusVector vector;

    /* Direct Index: supporting data, parsed from SignalGroups with the
    following indicator: 'metadata:labels:index: direct'. */
    Vector   signal_list;
    uint32_t offset; /* metadata/annotations/direct_index/length */
    uint32_t length; /* metadata/annotations/direct_index/offset */
} SimbusChannel;


typedef struct SimbusVectorIndex {
    SimbusVector* sbv;
    uint32_t      vi;
    struct {
        void*  map;
        size_t offset; /* Offset of _this_ vector in the map. */
        size_t size;   /* Upper bound of map. */
    } direct_index;
} SimbusVectorIndex;


/* adapter.c */
DLL_PUBLIC Adapter* simbus_adapter_create(
    Endpoint* endpoint, double bus_step_size);
DLL_PUBLIC void simbus_adapter_init_channel(
    AdapterModel* am, const char* channel_name, uint32_t expected_model_count);
DLL_PUBLIC void simbus_adapter_run(Adapter* adapter);

/* adapter_loopb.c (in parent directory) */
DLL_PUBLIC SimbusVectorIndex simbus_vector_lookup(
    SimulationSpec* sim, const char* vname, const char* sname);
DLL_PUBLIC void simbus_vector_binary_reset(SimulationSpec* sim);


#endif  // DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_
