// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_REDISPUBSUB_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_REDISPUBSUB_H_


#include <stdint.h>
#include <stdbool.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/queue.h>
#include <dse/platform.h>


typedef struct RedisPubSubChannel {
    char*       pub_key; /* TX direction. */
    char*       sub_key; /* RX direction. */
    /* Reference to the Adapter Channel linked to this Endpoint. */
    const char* channel_name;
} RedisPubSubChannel;

typedef struct RedisPubSubMessage {
    uint8_t*            buffer;
    uint32_t            length;
    RedisPubSubChannel* endpoint_channel;
} RedisPubSubMessage;

typedef struct RedisPubSubEndpoint {
    /* Redis properties. */
    const char*        path;     /* Unix Pipe. */
    const char*        hostname; /* TCP */
    int32_t            port;
    void*              ctx;
    uint32_t           client_id;
    /* RX properties. */
    void*              sub_ctx;
    struct event_base* sub_event_base;
    struct event*      sub_event_timeout;
    int                sub__argc;
    char**             sub__argv;
    bool               sub_active;
    double             recv_timeout;
    HashMap            endpoint_lookup;
    /* Recv on_message supporting properties. */
    queue_list_t       recv_msg_queue;
} RedisPubSubEndpoint;


/* redispubsub.c */
DLL_PRIVATE Endpoint* redispubsub_connect(const char* path,
    const char* hostname, int32_t port, uint32_t model_uid, bool bus_mode,
    double recv_timeout);
DLL_PRIVATE void      redispubsub_disconnect(Endpoint* endpoint);
DLL_PRIVATE int32_t   redispubsub_start(Endpoint* endpoint);
DLL_PRIVATE void*     redispubsub_create_channel(
        Endpoint* endpoint, const char* channel_name);
DLL_PRIVATE int32_t redispubsub_send_fbs(Endpoint* endpoint,
    void* endpoint_channel, void* buffer, uint32_t buffer_length,
    uint32_t model_uid);
DLL_PRIVATE int32_t redispubsub_recv_fbs(Endpoint* endpoint,
    const char** channel_name, uint8_t** buffer, uint32_t* buffer_length);
DLL_PRIVATE void    redispubsub_interrupt(Endpoint* endpoint);


#endif  // DSE_MODELC_ADAPTER_TRANSPORT_REDISPUBSUB_H_
