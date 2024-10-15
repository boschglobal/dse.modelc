// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_REDIS_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_REDIS_H_


#include <stdint.h>
#include <hiredis.h>
#include <hiredis/async.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/platform.h>


#define MAX_REDIS_KEY_SIZE 64


typedef struct RedisKeyDesc {
    char  endpoint[MAX_REDIS_KEY_SIZE];
    void* data;
} RedisKeyDesc;

typedef struct RedisEndpoint {
    /* Redis properties. */
    const char* path;     /* Unix Pipe. */
    const char* hostname; /* TCP */
    int32_t     port;
    void*       ctx;
    uint32_t    client_id;
    int         major_ver;
    int         minor_ver;

    /* RX properties. */
    double recv_timeout;

    /* Async properties. */
    redisAsyncContext* async_ctx;
    int                actx_connecting;
    struct event_base* base;
    size_t             reply_alloc_size;
    char*              reply_str;
    size_t             reply_len;
    int                reply_errno;

    /* Endpoints. */
    HashMap      push_hash;        /* Used in bus_mode, push to Models. */
    HashMap      notify_push_hash; /* Used in bus_mode, For notify only. */
    HashMap      pull_hash;
    RedisKeyDesc push;
    RedisKeyDesc pull;

    int     pull_argc;
    char**  pull_argv;
    size_t* pull_argvlen;
} RedisEndpoint;


/* redis.c */
DLL_PRIVATE Endpoint* redis_connect(const char* path, const char* hostname,
    int32_t port, uint32_t model_uid, bool bus_mode, double recv_timeout,
    bool async);
DLL_PRIVATE void*     redis_create_channel(
        Endpoint* endpoint, const char* channel_name);
DLL_PRIVATE int32_t redis_start(Endpoint* endpoint);
DLL_PRIVATE int32_t redis_send_fbs(Endpoint* endpoint, void* endpoint_channel,
    void* buffer, uint32_t buffer_length, uint32_t model_uid);
DLL_PRIVATE int32_t redis_recv_fbs(Endpoint* endpoint,
    const char** channel_name, uint8_t** buffer, uint32_t* buffer_length);
DLL_PRIVATE void    redis_interrupt(Endpoint* endpoint);
DLL_PRIVATE void    redis_disconnect(Endpoint* endpoint);
DLL_PRIVATE void    redis_register_notify_uid(
       Endpoint* endpoint, uint32_t notify_uid);


#endif  // DSE_MODELC_ADAPTER_TRANSPORT_REDIS_H_
