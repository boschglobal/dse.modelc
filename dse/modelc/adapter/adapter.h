// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_ADAPTER_H_
#define DSE_MODELC_ADAPTER_ADAPTER_H_


#include <stdint.h>
#include <stdbool.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/model.h>
#include <dse/platform.h>


#define ADAPTER_FALLBACK_CHANNEL "test"
#define UID_KEY_LEN              12


typedef struct SignalValue {
    char*    name;
    uint32_t uid;
    /* Double. */
    double   val;
    double   final_val;
    /* Binary. */
    void*    bin;
    uint32_t bin_size;
    uint32_t bin_buffer_size;
} SignalValue;

typedef struct SignalMap {
    const char*  name;
    SignalValue* signal;
} SignalMap;


typedef struct Channel {
    const char* name;
    void*       endpoint_channel;  // Reference to an Endpoint object
                                   // which represents this Channel.
    /* Signal properties. */
    HashMap     signal_values;       // Collection of SignalValue, key is name.
    char**      signal_values_keys;  // Cached result of hashmap_keys, update
                                     // on hashmap_set/remove.
    uint32_t    signal_values_length;
    SignalMap*  signal_values_map;  // Generated based on signal_values_keys,
                                    // cached speedup.
    /* Signal references (to controller/model API). */
    void*       ref__signal_value;
    bool        ref__convert_uint32;  // the type of the void* signal_value
    bool        ref__convert_int32;   // double otherwise.
    /* Bus properties. */
    SimpleSet*  model_register_set;
    SimpleSet*  model_ready_set;
    uint32_t    expected_model_count;  // Don't start until count reached,
                                       // could get more(?).
} Channel;


typedef struct Adapter Adapter;

typedef struct AdapterModel {
    /* Model properties. */
    uint32_t model_uid;
    double   model_time;
    double   stop_time;
    /* Channel properties. */
    HashMap  channels;       // Collection of Channel, key is name.
    char**   channels_keys;  // Cached result of hashmap_keys, update on
                             // hashmap_set/remove.
    uint32_t channels_length;

    Adapter* adapter;  // back reference ?? Model Instance ??
} AdapterModel;


typedef struct Adapter {
    bool          stop_request;
    HashMap       models; /* UID indexed hash of AdapterModel objects. */
    /* Bus properties. */
    bool          bus_mode;
    double        bus_time;
    double        bus_step_size;
    double        bus_time_correction;
    AdapterModel* bus_adapter_model;

    /* Endpoint container. */
    Endpoint* endpoint;
    /* Private object supporting the operation of the Adapter.*/
    void* private;
} Adapter;


/* adapter.c */
DLL_PRIVATE Adapter* adapter_create(void* endpoint);
DLL_PRIVATE Channel* adapter_init_channel(AdapterModel* am,
    const char* channel_name, const char** signal_name, uint32_t count);
DLL_PRIVATE void     adapter_connect(
        Adapter* adapter, SimulationSpec* sim, int retry_count);
DLL_PRIVATE void     adapter_register(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE int      adapter_ready(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE void     adapter_exit(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE void     adapter_interrupt(Adapter* adapter);
DLL_PRIVATE void     adapter_destroy_adapter_model(AdapterModel* am);
DLL_PRIVATE void     adapter_destroy(Adapter* adapter);
DLL_PRIVATE Channel* adapter_get_channel(
    AdapterModel* am, const char* channel_name);
DLL_PRIVATE SignalMap* adapter_get_signal_map(AdapterModel* am,
    const char* channel_name, const char** signal_name, uint32_t signal_count);
DLL_PRIVATE void adapter_dump_debug(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE void adapter_model_dump_debug(AdapterModel* am, const char* name);


#endif  // DSE_MODELC_ADAPTER_ADAPTER_H_
