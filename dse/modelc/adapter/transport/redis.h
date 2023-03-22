// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_REDIS_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_REDIS_H_


#include <stdint.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/platform.h>


typedef struct RedisChannel {
    char* endpoint_key;
    char* mbox_key;
    /* Reference to the Adapter Channel linked to this Endpoint. */
    void* adapter_channel;
} RedisChannel;

typedef struct RedisEndpoint {
    /* Redis properties. */
    const char* hostname;
    int32_t     port;
    void*       ctx;
    uint32_t    client_id;
    /* RX properties. */
    double      recv_timeout;
    HashMap     endpoint_lookup;

    /* Redis Command 'redis_recv_fbs' prebaked data. */
    double recv_fbs__timeout;
    char*  recv_fbs__timeout_str;
    int    recv_fbs__argc;
    char** recv_fbs__argv;

    RedisChannel* recv__endpoint_channel;
} RedisEndpoint;


/* redis.c */
DLL_PRIVATE Endpoint* redis_connect_tcp(const char* hostname, int32_t port,
    uint32_t model_uid, double recv_timeout);
DLL_PRIVATE void      redis_disconnect(Endpoint* endpoint);
DLL_PRIVATE int32_t   redis_start(Endpoint* endpoint);
DLL_PRIVATE void*     redis_create_channel(
        Endpoint* endpoint, const char* channel_name, void* adapter_channel);
DLL_PRIVATE int32_t redis_send_fbs(Endpoint* endpoint, void* endpoint_channel,
    void* buffer, uint32_t buffer_length, uint32_t model_uid);
DLL_PRIVATE int32_t redis_recv_fbs(Endpoint* endpoint, void** endpoint_channel,
    uint8_t** buffer, uint32_t* buffer_length);
DLL_PRIVATE void    redis_interrupt(Endpoint* endpoint);


#endif  // DSE_MODELC_ADAPTER_TRANSPORT_REDIS_H_
