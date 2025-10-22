// Copyright 2023, 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <dse/logger.h>
#include <dse/clib/collections/set.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/adapter/simbus/simbus_private.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/message.h>


#define ENV_SIMBUS_TRACE_FILE "SIMBUS_TRACEFILE"


#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(dse_schemas_fbs_channel, x)


DLL_PRIVATE bool __simbus_exit_run_loop__;


Adapter* simbus_adapter_create(Endpoint* endpoint, double bus_step_size)
{
    /* Force the endpoint to kind SIMBUS. */
    assert(endpoint);
    endpoint->kind = ENDPOINT_KIND_SIMBUS;

    /* Create adapter and manually create vtable. */
    Adapter* adapter = adapter_create(endpoint);
    assert(adapter);
    adapter->vtable = adapter_create_msg_vtable();

    /* Configure as Bus Adapter. */
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    adapter->bus_mode = true;
    adapter->bus_time =
        0.0 - bus_step_size;  // Will be set to 0.0 on first Sync.
    adapter->bus_step_size = bus_step_size;
    v->handle_message = simbus_handle_message;
    v->handle_notify_message = simbus_handle_notify_message;

    /* Create the Adapter Model object. */
    adapter->bus_adapter_model = calloc(1, sizeof(AdapterModel));
    int rc = hashmap_init(&adapter->bus_adapter_model->channels);
    if (rc) {
        if (errno == 0) errno = ENOMEM;
        log_fatal("Hashmap init failed for channels!");
    }
    adapter->bus_adapter_model->adapter = adapter;
    adapter->bus_adapter_model->model_uid = adapter->endpoint->uid;

    /* Benchmarking/Profiling. */
    simbus_profile_init(bus_step_size);

    /* Message trace. */
    char* _trace_file = getenv(ENV_SIMBUS_TRACE_FILE);
    if (_trace_file) {
        log_notice("Create trace file : %s", _trace_file);
        errno = 0;
        adapter->trace = fopen(_trace_file, "w");
        if (errno) {
            log_error("Unable to open SimBus trace file (%s)", _trace_file);
        }
    }

    return adapter;
}


uint32_t simbus_generate_uid_hash(const char* key)
{
    // FNV-1a hash (http://www.isthe.com/chongo/tech/comp/fnv/)
    size_t   len = strlen(key);
    uint32_t h = 2166136261UL; /* FNV_OFFSET 32 bit */
    for (size_t i = 0; i < len; ++i) {
        h = h ^ (unsigned char)key[i];
        h = h * 16777619UL; /* FNV_PRIME 32 bit */
    }
    return h;
}


void simbus_adapter_init_channel(
    AdapterModel* am, const char* channel_name, uint32_t expected_model_count)
{
    assert(am);

    /* Channel is created by previous call to adapter_init_channel ...*/
    Channel* ch = _get_channel(am, channel_name);
    assert(ch);
    /* Create the sets for tracking Sync messages. */
    ch->model_register_set = calloc(1, sizeof(SimpleSet));
    ch->model_ready_set = calloc(1, sizeof(SimpleSet));
    set_init(ch->model_register_set);
    set_init(ch->model_ready_set);
    /* Expected Model Count (array, element per channel) is the number of
    models expected to connect to this Adapter on each channel. The bus
    operation must wait until all expected models have sent a ModelRegister,
    during that time other messages also need to be processed. */
    ch->expected_model_count = expected_model_count;

    /* Set the Signal UID's, if specified, otherwise they are generated
       when processing SignalLookups. */
    _refresh_index(ch);
    for (uint32_t si = 0; si < ch->index.count; si++) {
        SignalValue* sv = _get_signal_value_byindex(ch, si);
        sv->uid = simbus_generate_uid_hash(sv->name);
        log_simbus("    [%u] uid=%u, name=%s", si, sv->uid, sv->name);
    }
}


void simbus_adapter_run(Adapter* adapter)
{
    assert(adapter);
    assert(adapter->endpoint);
    Endpoint* endpoint = adapter->endpoint;

    if (endpoint->start) endpoint->start(endpoint);

    __simbus_exit_run_loop__ = false;

    while (true) {
        const char* _msg_channel_name = NULL;
        bool        found = false; /* Found result is ignored. */
        wait_message(
            adapter, &_msg_channel_name, ns(MessageType_NONE), 0, &found);

        if (__simbus_exit_run_loop__) break;
    }

    log_simbus("exit run loop");

    /* Benchmarking/Profiling. */
    simbus_profile_print_benchmarks();
    simbus_profile_destroy();
}
