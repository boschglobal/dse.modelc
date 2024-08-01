// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_ENDPOINT_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_ENDPOINT_H_


#include <stdint.h>
#include <stdbool.h>
#include <dse/clib/collections/hashmap.h>


#define MAX_URI_LEN           2048
#define URI_SCHEME_DELIM      "://"

#define TRANSPORT_REDISPUBSUB "redispubsub"
#define TRANSPORT_REDIS       "redis"
#define TRANSPORT_MQ          "mq"
#define TRANSPORT_LOOPBACK    "loopback"


typedef struct Endpoint        Endpoint;
typedef struct msgpack_sbuffer msgpack_sbuffer;


typedef void* (*EndpointCreateChannelFunc)(
    Endpoint* endpoint, const char* channel_name);
typedef int32_t (*EndpointStartFunc)(Endpoint* endpoint);
typedef int32_t (*EndpointSendFbsFunc)(Endpoint* endpoint,
    void* endpoint_channel, void* buffer, uint32_t buffer_length,
    uint32_t model_uid);
typedef int32_t (*EndpointRecvFbsFunc)(Endpoint* endpoint,
    const char** channel_name, uint8_t** buffer, uint32_t* buffer_length);
typedef void (*EndpointInterruptFunc)(Endpoint* endpoint);
typedef void (*EndpointDisconnectFunc)(Endpoint* endpoint);


typedef enum EndpointKind {
    ENDPOINT_KIND_MESSAGE = 0,
    ENDPOINT_KIND_LOOPBACK,
    __ENDPOINT_KIND_SIZE__,
} EndpointKind;


typedef struct Endpoint {
    /* Endpoint properties. */
    uint32_t     uid;
    bool         stop_request;
    bool         bus_mode;
    EndpointKind kind;

    /* Callbacks */
    EndpointCreateChannelFunc create_channel;
    EndpointStartFunc         start;
    EndpointSendFbsFunc       send_fbs;
    EndpointRecvFbsFunc       recv_fbs;
    EndpointInterruptFunc     interrupt;
    EndpointDisconnectFunc    disconnect;

    /* Channel storage container. */
    HashMap endpoint_channels;

    /* Private object for endpoint specific data. */
    void* private;
} Endpoint;


/* endpoint.c */
DLL_PRIVATE Endpoint* endpoint_create(const char* transport, const char* uri,
    uint32_t uid, bool bus_mode, double timeout);


/* msgpack.c */
DLL_PRIVATE msgpack_sbuffer mp_encode_fbs(
    void* buffer, uint32_t buffer_length, const char* channel_name);
DLL_PRIVATE int32_t mp_decode_fbs(char* msg, int msg_len, uint8_t** buffer,
    uint32_t* buffer_length, Endpoint* endpoint, const char** channel_name);


#endif  // DSE_MODELC_ADAPTER_TRANSPORT_ENDPOINT_H_
