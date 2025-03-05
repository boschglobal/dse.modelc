// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <dlfcn.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/adapter.h>


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
        log_notice(
            "Load endpoint create function: %s", ADAPTER_CREATE_LOOPB_VTABLE);
        func = dlsym(handle, ADAPTER_CREATE_LOOPB_VTABLE);
        break;
    case ENDPOINT_KIND_MESSAGE:
        log_notice(
            "Load endpoint create function: %s", ADAPTER_CREATE_MSG_VTABLE);
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
