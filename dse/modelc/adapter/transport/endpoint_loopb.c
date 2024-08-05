// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/endpoint.h>


#define UNUSED(x) ((void)x)


/*
 *  endpoint_create
 *
 *  ** Special version supporting loopback operation in ModelC Runtime. **
 *
 *  Create an Endpoint object for the specified transport and URI.
 *
 *  Parameters
 *  ----------
 *  transport : const char*
 *      The type of transport to create.
 *  uri : const char*
 *      A URI which defines the transport to create.
 *  uid : uint32
 *      The UID (Unique Identifier) associated with the Model requesting the
 *      new Endpoint object.
 *  bus_mode : bool
 *      A TRUE value indicates that the Endpoint object will operate in Bus
 *      Mode (i.e. the requestor is a SimBus).
 *  timeout : double
 *      The timeout (seconds) that the Endpoint transport should use when
 *      waiting for messages.
 *
 *  Returns
 *  -------
 *      Endpoint (pointer to) : The created Endpoint object.
 *
 */
Endpoint* endpoint_create(const char* transport, const char* uri, uint32_t uid,
    bool bus_mode, double timeout)
{
    UNUSED(uri);
    UNUSED(uid);
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
