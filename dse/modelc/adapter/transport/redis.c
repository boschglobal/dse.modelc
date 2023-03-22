// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <hiredis.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/redis.h>
#include <dse/modelc/adapter/transport/endpoint.h>


#define REDIS_CONNECTION_TIMEOUT 5 /* Double, seconds. */
#define MAX_KEY_SIZE             64
#define UNUSED(x)                ((void)x)


static void redis_endpoint_destroy(Endpoint* endpoint)
{
    if (endpoint && endpoint->private) {
        RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
        if (redis_ep->ctx) redisFree(redis_ep->ctx);
        if (redis_ep->recv_fbs__timeout_str)
            free(redis_ep->recv_fbs__timeout_str);
        if (redis_ep->recv_fbs__argv) {
            if (redis_ep->recv_fbs__argv[0]) free(redis_ep->recv_fbs__argv[0]);
            free(redis_ep->recv_fbs__argv);
        }
        /* Release the endpoint_channels hashmap, this is only a lookup, so
        there is no need to release the contained objects. */
        hashmap_destroy(&redis_ep->endpoint_lookup);
        /* Release the RedisPubSubEndpoint object. */
        free(endpoint->private);
    }
    if (endpoint) {
        /* Release the endpoint_channels hashmap. */
        char**   keys = hashmap_keys(&endpoint->endpoint_channels);
        uint32_t count = hashmap_number_keys(endpoint->endpoint_channels);
        for (uint32_t i = 0; i < count; i++) {
            RedisChannel* ch =
                hashmap_get(&endpoint->endpoint_channels, keys[i]);
            if (ch->endpoint_key) free(ch->endpoint_key);
            if (ch->mbox_key) free(ch->mbox_key);
            free(ch);
        }
        hashmap_destroy(&endpoint->endpoint_channels);
        for (uint32_t _ = 0; _ < count; _++)
            free(keys[_]);
        free(keys);
    }
    free(endpoint);
}


Endpoint* redis_connect_tcp(
    const char* hostname, int32_t port, uint32_t model_uid, double recv_timeout)
{
    int rc;

    /* Endpoint. */
    Endpoint* endpoint = calloc(1, sizeof(Endpoint));
    if (endpoint == NULL) {
        log_error("Endpoint malloc failed!");
        goto error_clean_up;
    }
    endpoint->bus_mode = false;
    endpoint->create_channel = redis_create_channel;
    endpoint->start = redis_start;
    endpoint->send_fbs = redis_send_fbs;
    endpoint->recv_fbs = redis_recv_fbs;
    endpoint->interrupt = redis_interrupt;
    endpoint->disconnect = redis_disconnect;
    rc = hashmap_init_alt(&endpoint->endpoint_channels, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for endpoint->endpoint_channels!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }

    /* Redis Endpoint. */
    RedisEndpoint* redis_ep = calloc(1, sizeof(RedisEndpoint));
    if (redis_ep == NULL) {
        log_error("Redis_ep malloc failed!");
        goto error_clean_up;
    }
    redis_ep->hostname = hostname;
    redis_ep->port = port;
    redis_ep->recv_timeout = recv_timeout;
    rc = hashmap_init_alt(&redis_ep->endpoint_lookup, 32, NULL);
    if (rc) {
        log_error("Hashmap init failed for redis_ep->endpoint_lookup!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    endpoint->private = (void*)redis_ep;

    /* Redis connection. */
    redisReply*    reply;
    struct timeval timeout = { REDIS_CONNECTION_TIMEOUT, 0 };
    log_notice("  Redis:");
    log_notice("    hostname: %s", hostname);
    log_notice("    port: %d", port);
    redis_ep->ctx = redisConnectWithTimeout(hostname, port, timeout);
    redisContext* _ctx = (redisContext*)redis_ep->ctx;
    if (_ctx == NULL || _ctx->err) {
        if (_ctx) {
            log_notice("Connection error: %s", _ctx->errstr);
        } else {
            log_notice("Connection error: can't allocate Redis context");
        }
        log_error("Redis connect failed!");
        goto error_clean_up;
    }

    /* Model UID. */
    reply = redisCommand(redis_ep->ctx, "CLIENT ID");
    redis_ep->client_id = reply->integer;
    freeReplyObject(reply);
    if (model_uid) {
        endpoint->model_uid = model_uid;
    } else {
        endpoint->model_uid = redis_ep->client_id;
    }

    /* Log the Endpoint information. */
    log_notice("  Endpoint: ");
    log_notice("    Model UID: %i", endpoint->model_uid);
    log_notice("    Client ID: %i", redis_ep->client_id);

    return endpoint;

error_clean_up:
    redis_endpoint_destroy(endpoint);
    return NULL;
}


void redis_disconnect(Endpoint* endpoint)
{
    redis_endpoint_destroy(endpoint);
}


void* redis_create_channel(
    Endpoint* endpoint, const char* channel_name, void* adapter_channel)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;

    /* Create the RedisChannel object. */
    RedisChannel* endpoint_channel = calloc(1, sizeof(RedisChannel));
    assert(endpoint_channel);
    endpoint_channel->endpoint_key = calloc(1, MAX_KEY_SIZE);
    endpoint_channel->mbox_key = calloc(1, MAX_KEY_SIZE);
    snprintf(endpoint_channel->endpoint_key, MAX_KEY_SIZE - 1,
        "sbus.ch.%s.endpoint", channel_name);
    snprintf(endpoint_channel->mbox_key, MAX_KEY_SIZE - 1,
        "sbus.ch.%s.model.%i.mbox", channel_name, endpoint->model_uid);
    endpoint_channel->adapter_channel = adapter_channel;

    /* Add to Endpoint->endpoint_channels. */
    if (hashmap_set(&endpoint->endpoint_channels, channel_name,
            endpoint_channel) == NULL) {
        assert(0);
    }
    /* Set the lookup for mapping from PubSub key to endpoint_channel. */
    if (hashmap_set(&redis_ep->endpoint_lookup, endpoint_channel->endpoint_key,
            endpoint_channel) == NULL) {
        assert(0);
    }
    if (hashmap_set(&redis_ep->endpoint_lookup, endpoint_channel->mbox_key,
            endpoint_channel) == NULL) {
        assert(0);
    }

    log_notice("    Endpoint Key : %s", endpoint_channel->endpoint_key);
    log_notice("    MBox Key : %s", endpoint_channel->mbox_key);

    /* Return the created object, to the caller (which will be an Adapter). */
    return (void*)endpoint_channel;
}


int32_t redis_start(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;

    /* Build the 'redis_recv_fbs' prebaked data.
        argc[0]     = "BRPOP"
        argc[1]     = mbox_key_1
        argc[..N]   = mbox_key_N
        argc[N+1]   = recv_fbs__timeout
    */
    char**   keys = hashmap_keys(&endpoint->endpoint_channels);
    uint32_t count = hashmap_number_keys(endpoint->endpoint_channels);
    redis_ep->recv_fbs__argc = count + 2;
    redis_ep->recv_fbs__argv = calloc(redis_ep->recv_fbs__argc, sizeof(char*));
    /* The command. */
    redis_ep->recv_fbs__argv[0] = calloc(1, MAX_KEY_SIZE);
    snprintf(redis_ep->recv_fbs__argv[0], MAX_KEY_SIZE - 1, "BRPOP");
    /* The MBOX Keys. */
    for (uint32_t i = 0; i < count; i++) {
        RedisChannel* ch = hashmap_get(&endpoint->endpoint_channels, keys[i]);
        redis_ep->recv_fbs__argv[i + 1] = ch->mbox_key;
    }
    /* Timeout. */
    redis_ep->recv_fbs__timeout_str = calloc(1, MAX_KEY_SIZE);
    snprintf(redis_ep->recv_fbs__timeout_str, MAX_KEY_SIZE - 1, "%f",
        redis_ep->recv_timeout);
    redis_ep->recv_fbs__argv[0 + count + 1] = redis_ep->recv_fbs__timeout_str;
    /* Release the kragle. */
    for (uint32_t _ = 0; _ < count; _++)
        free(keys[_]);
    free(keys);

    return 0;
}

int32_t redis_send_fbs(Endpoint* endpoint, void* endpoint_channel, void* buffer,
    uint32_t buffer_length, uint32_t model_uid)
{
    UNUSED(model_uid);
    assert(endpoint);
    assert(endpoint->private);
    assert(endpoint_channel);

    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    assert(redis_ep->ctx);
    RedisChannel* ch = (RedisChannel*)endpoint_channel;

    redisReply* reply;
    reply = redisCommand(redis_ep->ctx, "LPUSH %s %b", ch->endpoint_key, buffer,
        (size_t)buffer_length);
    log_trace("redis_send_fbs: message sent");
    log_trace("redis_send_fbs:     channel=%s", ch->endpoint_key);
    freeReplyObject(reply);
    return 0;
}


int32_t redis_recv_fbs(Endpoint* endpoint, void** adapter_channel,
    uint8_t** buffer, uint32_t* buffer_length)
{
    uint32_t fbs_len;
    assert(endpoint);
    assert(endpoint->private);
    assert(adapter_channel);

    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    assert(redis_ep->ctx);

    log_debug("wait_message: redis_recv_fbs <-- ");
    for (int i = 0; i < redis_ep->recv_fbs__argc; i++)
        log_debug("wait_message:     %s", redis_ep->recv_fbs__argv[i]);
    redisReply* reply;
    reply = redisCommandArgv(redis_ep->ctx, redis_ep->recv_fbs__argc,
        (const char**)redis_ep->recv_fbs__argv, NULL);

    if (reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        /* No message ... timeout??? */
        log_debug("Wrong reply type (%d) or elements (%lu) -> Timeout !?!",
            reply->type, reply->elements);
        *adapter_channel = NULL;
        freeReplyObject(reply);
        return 0; /* Indicate that no message was received. */
    }
    /* Marshal data to caller. */
    fbs_len = reply->element[1]->len;
    /* Prepare the buffer, resize if necessary. */
    if (fbs_len > *buffer_length) {
        *buffer = realloc(*buffer, fbs_len);
        if (*buffer == NULL) {
            log_error("ERROR: malloc failed!");
            return -1;
        }
        *buffer_length = (size_t)fbs_len;
        log_debug(
            "INFO: Redis FBS Rx Buffer realloc() with size %u", *buffer_length);
    }
    memset(*buffer, 0, *buffer_length);
    memcpy(*buffer, reply->element[1]->str, (size_t)fbs_len);
    RedisChannel* endpoint_channel =
        hashmap_get(&redis_ep->endpoint_lookup, reply->element[0]->str);
    *adapter_channel = endpoint_channel->adapter_channel;
    freeReplyObject(reply);

    return fbs_len; /* The length of the FBS Message, not the buffer. */
    /* Channel index is returned via the channel_index pointer. */
}


void redis_interrupt(Endpoint* endpoint)
{
    assert(endpoint);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    UNUSED(redis_ep);
    exit(1);
}
