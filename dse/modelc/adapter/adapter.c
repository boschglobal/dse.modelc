// Copyright 2023, 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
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


#define UNUSED(x) ((void)x)


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


AdapterModel* adapter_get_model(Adapter* adapter, uint32_t model_uid)
{
    AdapterModelIndexItem* idx = vector_find(&adapter->models,
        &(AdapterModelIndexItem){ .uid = model_uid, .am = NULL }, 0, NULL);
    return idx ? idx->am : NULL;
}

static Channel* _create_channel(AdapterModel* am, const char* channel_name)
{
    assert(am);

    ChannelIndexItem* item = vector_find(&am->channels,
        &(ChannelIndexItem){ .name = channel_name, .ch = NULL }, 0, NULL);
    if (item) {
        assert(item->ch->name == channel_name);
        return item->ch;
    }
    /* Create a new Channel object. */
    Channel* ch = calloc(1, sizeof(Channel));
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
    /* Allocate index objects. Initialize uid vector for lookups. */
    ch->index.uid2sv_lookup =
        vector_make(sizeof(SignalValueIndexItem), 0, adapter_uid2sv_compar);

    /* Add the new Channel to the lookup vector. */
    ChannelIndexItem new_item = { .name = ch->name, .ch = ch };
    vector_push(&am->channels, &new_item);
    vector_sort(&am->channels);
    return ch;

error_clean_up:
    vector_reset(&ch->index.uid2sv_lookup);
    free(ch);
    return NULL;
}

Channel* adapter_init_channel(AdapterModel* am, const char* channel_name,
    const char** signal_name, uint32_t signal_count, void* mfc)
{
    assert(am);

    errno = 0;
    Channel* ch = _create_channel(am, channel_name);
    assert(ch);
    ch->mfc = mfc;
    ch->is_binary =
        mfc && ((ModelFunctionChannel*)mfc)->signal_value_binary != NULL;

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
    int rc = EINVAL;

    /* Send Notify message (ModelReady).

       A single Notify message will be constructed with all SV's from all
       Models included.

       Use the first instance to get a handle for the ready() method.
    */
    ModelInstanceSpec* mi = sim->instance_list;
    if (mi && mi->name) {
        if (adapter->vtable->ready) rc = adapter->vtable->ready(adapter);
    }
    if (rc != 0) log_error("Adapter ready error (%d)", rc);
    return rc;
}


int adapter_model_start(Adapter* adapter, SimulationSpec* sim)
{
    assert(sim);
    assert(adapter);
    assert(adapter->vtable);
    int rc = EINVAL;

    /* Wait for Notify message (ModelStart).

       Currently only a single Notify message will be received, and that
       will update all model instances.

       Use the first instance to get a handle for the ready() method.
    */
    ModelInstanceSpec* mi = sim->instance_list;
    if (mi && mi->name) {
        if (adapter->vtable->start) rc = adapter->vtable->start(adapter);
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
    if (am && vector_len(&am->channels)) {
        for (uint32_t i = 0; i < vector_len(&am->channels); i++) {
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
        vector_reset(&am->channels);
    }
    free(am);
}

void adapter_destroy(Adapter* adapter)
{
    if (adapter == NULL) return;

    vector_reset(&adapter->models);
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
    if (adapter->trace.file) {
        fclose(adapter->trace.file);
        adapter->trace.file = NULL;
    }
    if (adapter->trace.client_fd) {
        close(adapter->trace.client_fd);
        adapter->trace.client_fd = 0;
    }
    if (adapter->trace.server_fd) {
        close(adapter->trace.server_fd);
        adapter->trace.server_fd = 0;
    }
#ifdef _WIN32
#else
    if (adapter->trace.socket.path) {
        unlink(adapter->trace.socket.path);
        adapter->trace.socket.path = NULL;
    }
#endif
    free(adapter);
}


void adapter_model_dump_debug(AdapterModel* am, const char* name)
{
    log_simbus("Model Instance : %s", name);
    log_simbus("----------------");
    log_simbus("model_uid      : %u", am->model_uid);
    log_simbus("model_time     : %f", am->model_time);
    log_simbus("stop_time      : %f", am->stop_time);
    log_simbus("channel_count  : %u", vector_len(&am->channels));
    log_simbus("----------------");
    log_simbus("Channel Objects:");
    log_simbus("----------------");
    for (uint32_t channel_index = 0; channel_index < vector_len(&am->channels);
        channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        _refresh_index(ch);
        log_simbus("----------------------------------------");
        log_simbus("Channel [%u]:", channel_index);
        log_simbus("  name         : %s", ch->name);
        log_simbus("  signal_count : %u", ch->index.count);
        log_simbus("  signal_value :");
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue*          sv = ch->index.map[i].signal;
            double                value = sv->val;
            double                final_value = sv->final_val;
            ModelFunctionChannel* mfc = ch->mfc;
            if (mfc != NULL && mfc->scalar_sync.enabled &&
                mfc->signal_names != NULL && mfc->signal_value_double != NULL) {
                for (uint32_t local = 0; local < mfc->signal_count; local++) {
                    if (strcmp(mfc->signal_names[local], sv->name) == 0) {
                        value = final_value = mfc->signal_value_double[local];
                        break;
                    }
                }
            }
            log_simbus("    [%u] uid=%u, val=%f, final_val=%f, name=%s", i,
                sv->uid, value, final_value, sv->name);
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
