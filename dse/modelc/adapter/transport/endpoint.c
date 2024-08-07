// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/adapter/transport/redis.h>
#include <dse/modelc/adapter/transport/redispubsub.h>
#include <dse/modelc/adapter/transport/mq.h>


#define REDIS_PORT            6379
#define REDIS_URI_SCHEME      "redis" URI_SCHEME_DELIM
#define REDISASYNC_URI_SCHEME "redisasync" URI_SCHEME_DELIM
#define UNIX_URI_SCHEME       "unix" URI_SCHEME_DELIM


/**
 *  endpoint_create
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
 *  Example
 *  -------
 *  #include <dse/logger.h>
 *  #include <dse/modelc/adapter/transport/endpoint.h>
 *
 *  void run_func(void)
 *  {
 *      log_notice("Create the Endpoint object ...");
 *      Endpoint *endpoint = endpoint_create(
 *              "redispubsub", "redis://localhost:6379", 42, false, 60.0);
 *  }
 */
Endpoint* endpoint_create(const char* transport, const char* uri, uint32_t uid,
    bool bus_mode, double timeout)
{
    Endpoint*   endpoint = NULL;
    static char _uri[MAX_URI_LEN]; /* Other API's may refer to this data. */

    if ((strcmp(transport, TRANSPORT_REDISPUBSUB) == 0) ||
        (strcmp(transport, TRANSPORT_REDIS) == 0)) {
        /* Redis and Redis Pub/Sub. */
        /* Decode the URI. */
        strncpy(_uri, uri, MAX_URI_LEN - 1);
        if (strncmp(_uri, REDIS_URI_SCHEME, strlen(REDIS_URI_SCHEME)) == 0) {
            /* Parse according to: redis://host:[port] */
            char*   saveptr;
            char*   p = _uri + strlen(REDIS_URI_SCHEME);
            char*   hostname = strtok_r(p, ":", &saveptr);
            int32_t port = REDIS_PORT;
            p = strtok_r(NULL, ":", &saveptr);
            if (p) port = atol(p);
            /* Create this endpoint. */
            if (strcmp(transport, TRANSPORT_REDISPUBSUB) == 0) {
                endpoint = redispubsub_connect(
                    NULL, hostname, port, uid, bus_mode, timeout);
            } else {
                endpoint = redis_connect(
                    NULL, hostname, port, uid, bus_mode, timeout, false);
            }
        } else if (strncmp(_uri, REDISASYNC_URI_SCHEME,
                       strlen(REDISASYNC_URI_SCHEME)) == 0) {
            /* Parse according to: redisasync://host:[port] */
            char*   saveptr;
            char*   p = _uri + strlen(REDISASYNC_URI_SCHEME);
            char*   hostname = strtok_r(p, ":", &saveptr);
            int32_t port = REDIS_PORT;
            p = strtok_r(NULL, ":", &saveptr);
            if (p) port = atol(p);
            /* Create this endpoint. */
            endpoint = redis_connect(
                NULL, hostname, port, uid, bus_mode, timeout, true);
        } else if (strncmp(_uri, UNIX_URI_SCHEME, strlen(UNIX_URI_SCHEME)) ==
                   0) {
            /* Parse according to: unix:///tmp/redis/redis.sock */
            char* path = _uri + strlen(UNIX_URI_SCHEME);
            /* Create this endpoint. */
            if (strcmp(transport, TRANSPORT_REDISPUBSUB) == 0) {
                endpoint =
                    redispubsub_connect(path, NULL, 0, uid, bus_mode, timeout);
            } else {
                endpoint =
                    redis_connect(path, NULL, 0, uid, bus_mode, timeout, false);
            }
        } else {
            if (errno == 0) errno = EINVAL;
            log_error("ERROR: Incorrect Redis URI (%s)", uri);
            return NULL;
        }
    } else if (strcmp(transport, TRANSPORT_MQ) == 0) {
        /* Message Queue. */
        endpoint = mq_connect(uri, uid, bus_mode, timeout);
    } else if (strcmp(transport, TRANSPORT_LOOPBACK) == 0) {
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
