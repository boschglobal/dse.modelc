// Copyright 2023, 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/timer.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x)                   ((void)x)
#define ADAPTER_CREATE_MSG_VTABLE   "adapter_create_msg_vtable"
#define ADAPTER_CREATE_LOOPB_VTABLE "adapter_create_loopb_vtable"


static void* _create_vtable(Endpoint* endpoint)
{
    AdapterVTableCreate func = NULL;

    /* Get a handle to _this_ executable (self reference). */
    void* handle = dlopen(NULL, RTLD_LAZY);
    assert(handle);

    /* Select/create the adapter VTable. */
    switch (endpoint->kind) {
    case ENDPOINT_KIND_LOOPBACK:
        log_notice("Load endpoint create function: %s", ADAPTER_CREATE_LOOPB_VTABLE);
        func = dlsym(handle, ADAPTER_CREATE_LOOPB_VTABLE);
        break;
    case ENDPOINT_KIND_MESSAGE:
        log_notice("Load endpoint create function: %s", ADAPTER_CREATE_MSG_VTABLE);
        func = dlsym(handle, ADAPTER_CREATE_MSG_VTABLE);
        break;
    case ENDPOINT_KIND_SIMBUS:
        /* Special case, SimBus exec will directly create its vtable. */
        return NULL;
    default:
        log_fatal("Unsupported endpoint->kind (%d)", endpoint->kind);
    }
    if (func) {
        log_debug("Call create function ...");
        return func();
    } else {
        log_fatal("Endpoint create function not loaded");
    }
    return NULL;
}

Adapter* adapter_create(Endpoint* endpoint)
{
    assert(endpoint);
    Adapter* adapter;
    int      rc;

    errno = 0;
    adapter = calloc(1, sizeof(Adapter));
    if (adapter == NULL) {
        log_error("Adapter malloc failed!");
        goto error_clean_up;
    }
    adapter->vtable = _create_vtable(endpoint);
    if (adapter->vtable == NULL && endpoint->bus_mode == false) {
        log_error("adapter->vtable create failed!");
        goto error_clean_up;
    }

    /* Initialise supporting properties of the Adapter. */
    rc = hashmap_init(&adapter->models);
    if (rc) {
        log_error("Hashmap init failed for adapter_create!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    adapter->stop_request = false;
    adapter->endpoint = endpoint;

    return adapter;

error_clean_up:
    adapter_destroy(adapter);
    return NULL;
}


SignalMap* adapter_get_signal_map(AdapterModel* am, const char* channel_name,
    const char** signal_name, uint32_t signal_count)
{
    /* This will generate an array of map objects. The indexing will
       match the callers signal_name array, the map holds a pointer
       to the signal value of the adapter (the consolidation point).

       This way, a Model Function can get a _subset_ of all channel signals.

       Caller to free SignalMap, but not referenced SignalValue object.
    */
    Channel* ch = _get_channel(am, channel_name);
    assert(ch);
    return _get_signal_value_map(ch, signal_name, signal_count);
}


static void _update_channels_keys(AdapterModel* am)
{
    if (am->channels_keys) {
        for (uint32_t _ = 0; _ < am->channels_length; _++)
            free(am->channels_keys[_]);
        free(am->channels_keys);
        am->channels_keys = NULL;
    }
    am->channels_keys = hashmap_keys(&am->channels);
    am->channels_length = hashmap_number_keys(am->channels);
}

static Channel* _create_channel(AdapterModel* am, const char* channel_name)
{
    assert(am);

    Channel* ch = hashmap_get(&am->channels, channel_name);
    if (ch) {
        assert(ch->name == channel_name);
        return ch;
    }
    /* Create a new Channel object. */
    ch = calloc(1, sizeof(Channel));
    ch->name = channel_name;
    int rc = hashmap_init(&ch->signal_values);
    if (rc) {
        log_error("Hashmap init failed for _create_channel.signal_values!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    /* Set the endpoint object. */
    if (am->adapter && am->adapter->endpoint) {
        if (am->adapter->endpoint->create_channel) {
            ch->endpoint_channel = am->adapter->endpoint->create_channel(
                am->adapter->endpoint, ch->name);
        }
    }

    /* Add the new Channel to the hashmap. */
    if (hashmap_set(&am->channels, ch->name, ch)) {
        _update_channels_keys(am);
        return ch;
    }
    log_error("Adapter _create_channel failed to create new Channel object!");
    goto error_clean_up;

error_clean_up:
    free(ch);
    return NULL;
}

Channel* adapter_init_channel(AdapterModel* am, const char* channel_name,
    const char** signal_name, uint32_t signal_count)
{
    assert(am);

    errno = 0;
    Channel* ch = _create_channel(am, channel_name);
    assert(ch);

    /* Initialise the Signal properties. */
    for (uint32_t i = 0; i < signal_count; i++) {
        _get_signal_value(ch, signal_name[i]); /* Creates if missing. */
    }
    _refresh_index(ch);

    /* Return the channel object so that caller can configure other properties.
     */
    return ch;
}


Channel* adapter_get_channel(AdapterModel* am, const char* channel_name)
{
    /* Public interface to get channel, decoupled. */
    return _get_channel(am, channel_name);
}


void adapter_connect(Adapter* adapter, SimulationSpec* sim, int retry_count)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    if (adapter->vtable->connect == NULL) return;

    int rc = 0;
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        ModelInstancePrivate* mip = mi->private;
        AdapterModel*         am = mip->adapter_model;
        rc |= adapter->vtable->connect(am, sim, retry_count);
    }
    if (rc != 0) log_error("Adapter connect error (%d)", rc);
}


void adapter_register(Adapter* adapter, SimulationSpec* sim)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    if (adapter->vtable->register_ == NULL) return;

    int rc = 0;
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        ModelInstancePrivate* mip = mi->private;
        AdapterModel*         am = mip->adapter_model;
        rc |= adapter->vtable->register_(am);
    }
    if (rc != 0) log_error("Adapter register error (%d)", rc);
}


int adapter_model_ready(Adapter* adapter, SimulationSpec* sim)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    int rc = 0;

    /* Send Notify message (ModelReady).

       A single Notify message will be constructed with all SV's from all
       Models included.
    */
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        ModelInstancePrivate* mip = mi->private;
        AdapterModel*         am = mip->adapter_model;
        if (adapter->vtable->ready) rc |= adapter->vtable->ready(am);
    }
    if (rc != 0) log_error("Adapter ready error (%d)", rc);
    return rc;
}


int adapter_model_start(Adapter* adapter, SimulationSpec* sim)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    int rc = 0;

    /* Wait for Notify message (ModelStart).

       Currently only a single Notify message will be received, and that
       will update all model instances.
    */
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        ModelInstancePrivate* mip = mi->private;
        AdapterModel*         am = mip->adapter_model;
        if (adapter->vtable->start) rc |= adapter->vtable->start(am);
    }
    if (rc != 0) log_error("Adapter start error (%d)", rc);
    return rc;
}


int adapter_ready(Adapter* adapter, SimulationSpec* sim)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    int rc = 0;

    /* Send Notify message (ModelReady). */
    rc = adapter_model_ready(adapter, sim);

    /* Wait for Notify message (ModelStart). */
    rc |= adapter_model_start(adapter, sim);

    return rc;
}


void adapter_exit(Adapter* adapter, SimulationSpec* sim)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    if (adapter->vtable->exit == NULL) return;

    int rc = 0;
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        ModelInstancePrivate* mip = mi->private;
        AdapterModel*         am = mip->adapter_model;
        rc |= adapter->vtable->exit(am);
    }
    if (rc != 0) log_error("Adapter exit error (%d)", rc);
}


void adapter_interrupt(Adapter* adapter)
{
    if (adapter) adapter->stop_request = true;

    if (adapter && adapter->endpoint) {
        Endpoint* endpoint = adapter->endpoint;
        if (endpoint->interrupt) endpoint->interrupt(endpoint);
    }
}


static void _destroy_signal_value(void* map_item, void* data)
{
    UNUSED(data);

    SignalValue* sv = map_item;
    if (sv) {
        if (sv->name) free(sv->name);
        if (sv->bin) free(sv->bin);
        // Hashmap will free sv object.
    }
}

void adapter_destroy_adapter_model(AdapterModel* am)
{
    if (am && am->channels_length) {
        for (uint32_t i = 0; i < am->channels_length; i++) {
            Channel* ch = _get_channel_byindex(am, i);
            hashmap_destroy_ext(
                &ch->signal_values, _destroy_signal_value, NULL);
            _destroy_index(ch);
            if (ch->model_register_set) {
                set_destroy(ch->model_register_set);
                free(ch->model_register_set);
            }
            if (ch->model_ready_set) {
                set_destroy(ch->model_ready_set);
                free(ch->model_ready_set);
            }
            free(ch);
        }
        hashmap_destroy(&am->channels);
        if (am->channels_keys) {
            for (uint32_t _ = 0; _ < am->channels_length; _++)
                free(am->channels_keys[_]);
        }
        free(am->channels_keys);
        am->channels_length = 0;
    }
    free(am);
}

void adapter_destroy(Adapter* adapter)
{
    if (adapter == NULL) return;

    hashmap_destroy(&adapter->models);
    if (adapter->endpoint) {
        Endpoint* endpoint = adapter->endpoint;
        endpoint->disconnect(endpoint);
    }
    if (adapter->bus_adapter_model) {
        adapter_destroy_adapter_model(adapter->bus_adapter_model);
    }
    if (adapter->vtable) {
        if (adapter->vtable->destroy) {
            adapter->vtable->destroy(adapter);
        }
        free(adapter->vtable);
        adapter->vtable = NULL;
    }
    free(adapter);
}


void adapter_model_dump_debug(AdapterModel* am, const char* name)
{
    log_simbus("Model Instance : %s", name);
    log_simbus("----------------");
    log_simbus("model_uid      : %u", am->model_uid);
    log_simbus("model_time     : %f", am->model_time);
    log_simbus("stop_time      : %f", am->stop_time);
    log_simbus("channel_count  : %u", am->channels_length);
    log_simbus("----------------");
    log_simbus("Channel Objects:");
    log_simbus("----------------");
    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        _refresh_index(ch);
        log_simbus("----------------------------------------");
        log_simbus("Channel [%u]:", channel_index);
        log_simbus("  name         : %s", ch->name);
        log_simbus("  signal_count : %u", ch->index.count);
        log_simbus("  signal_value :");
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = ch->index.map[i].signal;
            log_simbus("    [%u] uid=%u, val=%f, final_val=%f, name=%s", i,
                sv->uid, sv->val, sv->final_val, sv->name);
        }
    }
}


void adapter_dump_debug(Adapter* adapter, SimulationSpec* sim)
{
    assert(adapter);

    /* Dump Adapter properties. */
    log_simbus("========================================");
    log_simbus("Adapter Dump");
    log_simbus("========================================");
    log_simbus("Adapter Objects:");
    log_simbus("----------------------------------------");
    for (ModelInstanceSpec* mi = sim->instance_list; mi && mi->name; mi++) {
        ModelInstancePrivate* mip = mi->private;
        AdapterModel*         am = mip->adapter_model;
        adapter_model_dump_debug(am, mi->name);
    }
    log_simbus("========================================");
}
