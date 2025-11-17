// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/transport/endpoint.h>


#define UNUSED(x) ((void)x)


AdapterVTable* adapter_create_msg_vtable(void)
{
    return NULL;
}


Endpoint* endpoint_create(const char* transport, const char* uri, uint32_t uid,
    bool bus_mode, double timeout)
{
    UNUSED(uri);
    UNUSED(timeout);
    Endpoint* endpoint = NULL;

    if (strcmp(transport, TRANSPORT_LOOPBACK) == 0) {
        /* Loopback - may also be created outside of this function. */
        endpoint = calloc(1, sizeof(Endpoint));
        endpoint->kind = ENDPOINT_KIND_LOOPBACK;
        endpoint->uid = uid;
        endpoint->bus_mode = bus_mode;
        endpoint->disconnect = (EndpointDisconnectFunc)free;
    } else {
        /* Unknown transport. */
        if (errno == 0) errno = EINVAL;
        log_error("ERROR: unknown transport! (%s)", transport);
        return NULL;
    }

    return endpoint;
}
