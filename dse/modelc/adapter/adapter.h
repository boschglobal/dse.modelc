// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_ADAPTER_H_
#define DSE_MODELC_ADAPTER_ADAPTER_H_


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/model.h>
#include <dse/platform.h>


#define ADAPTER_FALLBACK_CHANNEL "test"
#define UID_KEY_LEN              12


typedef struct Adapter      Adapter;
typedef struct AdapterModel AdapterModel;


/*
Adapter Interface
-----------------
*/

typedef int (*AdapterConnect)(
    AdapterModel* am, SimulationSpec* sim, int retry_count);
typedef int (*AdapterRegister)(AdapterModel* am);
typedef int (*AdapterReady)(Adapter* adapter);
typedef int (*AdapterStart)(Adapter* adapter);
typedef int (*AdapterExit)(AdapterModel* am);
typedef void (*AdapterDestroy)(Adapter* a);

typedef struct AdapterVTable {
    AdapterConnect  connect;
    AdapterRegister register_;
    AdapterReady    ready;
    AdapterStart    start;
    AdapterExit     exit;
    AdapterDestroy  destroy;
} AdapterVTable;

typedef AdapterVTable* (*AdapterVTableCreate)(void);


/*
Adapter Objects
---------------
*/

typedef struct SignalValue {
    char*    name;
    uint32_t uid;
    uint32_t vector_index;
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
    void*       endpoint_channel;  // Reference to an Endpoint object.

    /* Signal properties. */
    HashMap signal_values;  // map{name:SignalValue}
    struct {
        /* Index supporting the signal_values hash. */
        char**     names;
        uint32_t   count;
        uint32_t   hash_code;
        /* Map used by _this_ channel (contains all signals). */
        SignalMap* map;
        /* Hashmap Lookup. */
        HashMap    uid2sv_lookup;
    } index;

    /* Bus properties. */
    SimpleSet* model_register_set;
    SimpleSet* model_ready_set;
    uint32_t   expected_model_count;
} Channel;


typedef struct AdapterModel {
    /* Model properties. */
    uint32_t model_uid;
    double   model_time;
    double   stop_time;

    /* Channel properties. */
    HashMap  channels;  // map{name: Channel}.
    char**   channels_keys;
    uint32_t channels_length;

    /* Reference objects. */
    Adapter* adapter;

    /* Benchmarking/Profiling. */
    struct timespec bench_notifyrecv_ts;
    uint64_t        bench_steptime_ns;
} AdapterModel;


typedef struct Adapter {
    bool    stop_request;
    HashMap models;  // map{uid:AdapterModel}

    /* Adapter vtable, type may be extended. */
    AdapterVTable* vtable;

    /* Bus properties. */
    bool          bus_mode;
    double        bus_time;
    double        bus_step_size;
    double        bus_time_correction;
    AdapterModel* bus_adapter_model;

    /* Endpoint container. */
    Endpoint* endpoint;

    /* Benchmarking/Profiling. */
    struct timespec bench_notifysend_ts;

    /* Message trace. */
    FILE* trace;
} Adapter;


/* adapter.c */
DLL_PRIVATE Adapter* adapter_create(Endpoint* endpoint);
DLL_PRIVATE Channel* adapter_init_channel(AdapterModel* am,
    const char* channel_name, const char** signal_name, uint32_t count);
DLL_PRIVATE void     adapter_connect(
        Adapter* adapter, SimulationSpec* sim, int retry_count);
DLL_PRIVATE void     adapter_register(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE int      adapter_ready(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE int      adapter_model_ready(Adapter* adapter, SimulationSpec* sim);
DLL_PRIVATE int      adapter_model_start(Adapter* adapter, SimulationSpec* sim);
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

/* adapter_msg.c */
DLL_PUBLIC AdapterVTable* adapter_create_msg_vtable(void);

/* adapter_loopb.c */
DLL_PUBLIC AdapterVTable* adapter_create_loopb_vtable(void);


#endif  // DSE_MODELC_ADAPTER_ADAPTER_H_
