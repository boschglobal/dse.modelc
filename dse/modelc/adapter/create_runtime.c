// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/adapter.h>


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
    adapter->vtable = adapter_create_loopb_vtable();
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
