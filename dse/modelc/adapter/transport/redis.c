// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <msgpack.h>
#include <hiredis/adapters/libevent.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/transport/redis.h>
#include <dse/modelc/adapter/transport/endpoint.h>


#define REDIS_VERSION_FIELD      "redis_version:"
#define REDIS_CONNECTION_TIMEOUT 5 /* Double, seconds. */
#define MAX_KEY_SIZE             64
#define DESC_KEY_LEN             12
#define UNUSED(x)                ((void)x)


static void redis_endpoint_destroy(Endpoint* endpoint)
{
    if (endpoint && endpoint->private) {
        RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
        /* Redis objects.*/
        if (redis_ep->ctx) redisFree(redis_ep->ctx);
        if (redis_ep->async_ctx) {
            if (redis_ep->reply_str) free(redis_ep->reply_str);
            redisAsyncFree(redis_ep->async_ctx);
            if (redis_ep->base) event_base_free(redis_ep->base);
        }
        /* Release the redis_ep->push_hash hashmap. */
        char**   keys = hashmap_keys(&redis_ep->push_hash);
        uint32_t count = hashmap_number_keys(redis_ep->push_hash);
        for (uint32_t i = 0; i < count; i++) {
            RedisKeyDesc* _ = hashmap_get(&redis_ep->push_hash, keys[i]);
            free(_);
        }
        hashmap_destroy(&redis_ep->push_hash);
        for (uint32_t _ = 0; _ < count; _++)
            free(keys[_]);
        free(keys);
        /* Free the Redis endpoint. */
        free(endpoint->private);
    }
    if (endpoint) {
        /* Contains pointers to const char* so only destroy the hashmap. */
        hashmap_destroy(&endpoint->endpoint_channels);
    }
    free(endpoint);
}

static void _redis__async_connect(const redisAsyncContext* c, int status)
{
    UNUSED(status);
    RedisEndpoint* redis_ep = c->data;
    redis_ep->actx_connecting = 0;
}

static void _check_free_reply(redisReply* reply)
{
    if (reply && reply->type == REDIS_REPLY_ERROR) {
        log_error("REDIS_REPLY_ERROR : %s", reply->str);
    }
    freeReplyObject(reply);
}


Endpoint* redis_connect(const char* path, const char* hostname, int32_t port,
    uint32_t model_uid, bool bus_mode, double recv_timeout, bool async)
{
    int rc;

    /* Endpoint. */
    Endpoint* endpoint = calloc(1, sizeof(Endpoint));
    if (endpoint == NULL) {
        log_error("Endpoint malloc failed!");
        goto error_clean_up;
    }
    endpoint->bus_mode = bus_mode;
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
    redis_ep->path = path;
    redis_ep->hostname = hostname;
    redis_ep->port = port;
    redis_ep->recv_timeout = recv_timeout;
    rc = hashmap_init_alt(&redis_ep->push_hash, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for redis_ep->push_hash!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    endpoint->private = (void*)redis_ep;

    /* Redis connection. */
    struct timeval timeout = { REDIS_CONNECTION_TIMEOUT, 0 };
    log_notice("  Redis:");
    log_notice("    path: %s", redis_ep->path);
    log_notice("    hostname: %s", redis_ep->hostname);
    log_notice("    port: %d", redis_ep->port);
    if (redis_ep->hostname) {
        redis_ep->ctx = redisConnectWithTimeout(
            redis_ep->hostname, redis_ep->port, timeout);
    } else if (redis_ep->path) {
        redis_ep->ctx = redisConnectUnixWithTimeout(redis_ep->path, timeout);
    } else {
        log_fatal("Redis connect parameters not complete.");
    }
    redisContext* _ctx = (redisContext*)redis_ep->ctx;
    if (_ctx == NULL || _ctx->err) {
        if (_ctx) {
            log_notice("Connection error: %s", _ctx->errstr);
        } else {
            log_error("Connection error: can't allocate Redis context");
        }
        goto error_clean_up;
    }

    if (async) {
        redis_ep->async_ctx =
            redisAsyncConnect(redis_ep->hostname, redis_ep->port);
        if (redis_ep->async_ctx->err) log_fatal("XXXX 1");
        redis_ep->actx_connecting = 1;
        redis_ep->async_ctx->data = redis_ep;
        redis_ep->base = event_base_new();
        redisLibeventAttach(redis_ep->async_ctx, redis_ep->base);
        redisAsyncSetConnectCallback(
            redis_ep->async_ctx, _redis__async_connect);
    }

    /* Redis Version. */
    redisReply* reply = redisCommand(redis_ep->ctx, "INFO");
    if (reply->type == REDIS_REPLY_STRING) {
        char* v = strstr(reply->str, REDIS_VERSION_FIELD);
        if (v) {
            v += strlen(REDIS_VERSION_FIELD);
            redis_ep->major_ver = strtol(v, &v, 10);
            redis_ep->minor_ver = strtol(v + 1, NULL, 10);
        }
    }
    freeReplyObject(reply);
    log_notice("    major version: %d", redis_ep->major_ver);
    log_notice("    minor version: %d", redis_ep->minor_ver);

    /* Model UID. */
    reply = redisCommand(redis_ep->ctx, "CLIENT ID");
    redis_ep->client_id = reply->integer;
    freeReplyObject(reply);
    if (model_uid) {
        endpoint->uid = model_uid;
    } else {
        endpoint->uid = redis_ep->client_id;
    }

    /* Configure the Endpoint. */
    if (endpoint->bus_mode) {
        /* SimBus. Push EP are allocated/assigned in Hash. */
        snprintf(redis_ep->pull.endpoint, MAX_KEY_SIZE, "dse.simbus");
        /* Log the Endpoint information. */
        log_notice("  Endpoint: ");
        log_notice("    Model UID: %i", endpoint->uid);
        log_notice("    Pull Endpoint: %s", redis_ep->pull.endpoint);
    } else {
        /* Model. */
        snprintf(redis_ep->push.endpoint, MAX_KEY_SIZE, "dse.simbus");
        snprintf(redis_ep->pull.endpoint, MAX_KEY_SIZE, "dse.model.%d",
            endpoint->uid);
        /* Log the Endpoint information. */
        log_notice("  Endpoint: ");
        log_notice("    Model UID: %i", endpoint->uid);
        log_notice("    Push Endpoint: %s", redis_ep->push.endpoint);
        log_notice("    Pull Endpoint: %s", redis_ep->pull.endpoint);
    }

    return endpoint;

error_clean_up:
    redis_endpoint_destroy(endpoint);
    return NULL;
}


void* redis_create_channel(Endpoint* endpoint, const char* channel_name)
{
    assert(endpoint);
    assert(endpoint->private);

    /* Maintain a list of channel_names. There is no associated metadata
       so only store the channel name pointer. */
    if (hashmap_get(&endpoint->endpoint_channels, channel_name) == NULL) {
        hashmap_set(
            &endpoint->endpoint_channels, channel_name, (void*)channel_name);
        log_notice("    Endpoint Channel : %s", channel_name);
    }

    /* Return the created object, to the caller (which will be an Adapter). */
    return (void*)channel_name;
}


int32_t redis_start(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    assert(redis_ep->ctx);

    /* Setup the Push/Pull Key (remove previous state). */
    redisReply* _ =
        redisCommand(redis_ep->ctx, "DEL %s", redis_ep->pull.endpoint);
    _check_free_reply(_);
    if (endpoint->bus_mode) {
        /* PUSH are Models, handled by hash. */
        /* PULL is SimBus.*/
        log_debug("Redis Endpoint: PULL: %s (SimBus)", redis_ep->pull.endpoint);
    } else {
        log_debug("Redis Endpoint: PULL: %s (Model)", redis_ep->pull.endpoint);
        log_debug("Redis Endpoint: PUSH: %s (SimBus)", redis_ep->push.endpoint);
    }

    return 0;
}


static RedisKeyDesc* _model_push_desc(Endpoint* endpoint, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;

    /* Models always push to SimBus. */
    if (!endpoint->bus_mode) return &redis_ep->push;

    /* Locate the push key from the hash. */
    RedisKeyDesc* push_desc = NULL;
    char          hash_key[DESC_KEY_LEN];
    snprintf(hash_key, DESC_KEY_LEN - 1, "%d", model_uid);
    push_desc = hashmap_get(&redis_ep->push_hash, hash_key);
    if (push_desc) return push_desc;

    /* Create a new entry (will be a Model Push). */
    push_desc = calloc(1, sizeof(RedisKeyDesc));
    assert(push_desc);
    snprintf(push_desc->endpoint, MAX_KEY_SIZE, "dse.model.%d", model_uid);
    hashmap_set(&redis_ep->push_hash, hash_key, push_desc);

    return push_desc;
}

static RedisKeyDesc* _get_push_key(Endpoint* endpoint, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);

    RedisKeyDesc* key_desc = _model_push_desc(endpoint, model_uid);
    assert(key_desc);
    return key_desc;
}

int32_t redis_send_fbs(Endpoint* endpoint, void* endpoint_channel, void* buffer,
    uint32_t buffer_length, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    int            rc = 0;

    /* Construct the message payload. */
    const char*     channel_name = endpoint_channel;
    msgpack_sbuffer sbuf = mp_encode_fbs(buffer, buffer_length, channel_name);

    /* Send the message/datagram. */
    if (endpoint_channel) {
        RedisKeyDesc* push_desc = _get_push_key(endpoint, model_uid);
        log_trace("Redis send: endpoint:%s, length: %d", push_desc->endpoint,
            sbuf.size);
        redisReply* _ = redisCommand(redis_ep->ctx, "LPUSH %s %b",
            push_desc->endpoint, (void*)sbuf.data, (size_t)sbuf.size);
        _check_free_reply(_);
    } else {
        if (endpoint->bus_mode) {
            /* Sending a Notify Message to each model. */
            char**   keys = hashmap_keys(&redis_ep->push_hash);
            uint32_t count = hashmap_number_keys(redis_ep->push_hash);
            for (uint32_t i = 0; i < count; i++) {
                RedisKeyDesc* push_desc =
                    hashmap_get(&redis_ep->push_hash, keys[i]);
                log_trace("Redis send: endpoint:%s, length: %d",
                    push_desc->endpoint, sbuf.size);
                if (redis_ep->async_ctx) {
                    redisAsyncCommand(redis_ep->async_ctx, NULL, NULL,
                        "LPUSH %s %b", push_desc->endpoint, (void*)sbuf.data,
                        (size_t)sbuf.size);
                } else {
                    redisReply* _ = redisCommand(redis_ep->ctx, "LPUSH %s %b",
                        push_desc->endpoint, (void*)sbuf.data,
                        (size_t)sbuf.size);
                    _check_free_reply(_);
                }
            }
            for (uint32_t _ = 0; _ < count; _++)
                free(keys[_]);
            free(keys);

        } else {
            RedisKeyDesc* push_desc = _get_push_key(endpoint, model_uid);
            log_trace("Redis send: endpoint:%s, length: %d",
                push_desc->endpoint, sbuf.size);
            if (redis_ep->async_ctx) {
                redisAsyncCommand(redis_ep->async_ctx, NULL, NULL,
                    "LPUSH %s %b", push_desc->endpoint, (void*)sbuf.data,
                    (size_t)sbuf.size);
            } else {
                redisReply* _ = redisCommand(redis_ep->ctx, "LPUSH %s %b",
                    push_desc->endpoint, (void*)sbuf.data, (size_t)sbuf.size);
                _check_free_reply(_);
            }
        }
    }

    /* Release the encoded datagram. */
    msgpack_sbuffer_destroy(&sbuf);

    return rc;
}


static void _redis_async_brpop(redisAsyncContext* c, void* r, void* privdata)
{
    UNUSED(c);
    redisReply*    reply = r;
    RedisEndpoint* redis_ep = privdata;

    if (reply->type == REDIS_REPLY_ERROR) {
        log_debug("REDIS_REPLY_ERROR : %s", reply->str);
    }
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
        redis_ep->reply_len = reply->element[1]->len;
        if (redis_ep->reply_len > redis_ep->reply_alloc_size) {
            free(redis_ep->reply_str);
            redis_ep->reply_alloc_size = redis_ep->reply_len;
            redis_ep->reply_str = malloc(redis_ep->reply_alloc_size);
        }
        memcpy(
            redis_ep->reply_str, reply->element[1]->str, redis_ep->reply_len);
    } else {
        if (reply->type == REDIS_REPLY_NIL) errno = ETIMEDOUT;
        if (errno == 0) errno = ENODATA;
        redis_ep->reply_errno = errno;
    }

    event_base_loopbreak(redis_ep->base);
}

int32_t redis_recv_fbs(Endpoint* endpoint, const char** channel_name,
    uint8_t** buffer, uint32_t* buffer_length)
{
    assert(endpoint);
    assert(endpoint->private);
    assert(channel_name);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    assert(redis_ep->ctx);

    int timeout_counter = (int32_t)redis_ep->recv_timeout + 1;


    *channel_name = NULL; /* Set the default return condition. */
    redisReply* reply = NULL;

    if (redis_ep->reply_str == NULL && redis_ep->async_ctx) {
        redis_ep->reply_alloc_size = 1000;
        redis_ep->reply_str = malloc(redis_ep->reply_alloc_size);
    }

    while (--timeout_counter) {
        if (endpoint->stop_request) {
            /* Request to exit. */
            return 0;
        }

        redis_ep->reply_errno = 0;
        redis_ep->reply_len = 0;

        /* Timed receive for 1 second (at a time). Normally no performance hit
         * in doing this loop as we expect messages on a shorter time frame.
         */
        if (redis_ep->async_ctx) {
            if (redis_ep->major_ver >= 6) {
                redisAsyncCommand(redis_ep->async_ctx, _redis_async_brpop,
                    redis_ep, "BRPOP %s %f", redis_ep->pull.endpoint, 1.0);
            } else {
                redisAsyncCommand(redis_ep->async_ctx, _redis_async_brpop,
                    redis_ep, "BRPOP %s %d", redis_ep->pull.endpoint, 1);
            }
            event_base_dispatch(redis_ep->base);
        } else {
            if (redis_ep->major_ver >= 6) {
                reply = redisCommand(
                    redis_ep->ctx, "BRPOP %s %f", redis_ep->pull.endpoint, 1.0);
            } else {
                reply = redisCommand(
                    redis_ep->ctx, "BRPOP %s %d", redis_ep->pull.endpoint, 1);
            }
            if (reply->type == REDIS_REPLY_ERROR) {
                log_debug("REDIS_REPLY_ERROR : %s", reply->str);
            }
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
                redis_ep->reply_len = reply->element[1]->len;
                redis_ep->reply_str = reply->element[1]->str;
            } else {
                if (reply->type == REDIS_REPLY_NIL) errno = ETIMEDOUT;
                if (errno == 0) errno = ENODATA;
                redis_ep->reply_errno = errno;
                freeReplyObject(reply);
                reply = NULL;
                redis_ep->reply_len = 0;
                redis_ep->reply_str = NULL;
            }
        }

        if (redis_ep->reply_errno) {
            if (redis_ep->reply_errno == ETIMEDOUT) {
                /* Timeout on the call, next loop. */
                continue;
            }
            log_fatal("redis command BRPOP failed!");
            return -1; /* -ve number indicates failure to receive. */
        }
        break;
    }
    if (timeout_counter == 0) {
        log_trace("redis_recv_fbs: no message (timeout)");
        errno = ETIME;
        if (reply) freeReplyObject(reply);
        return -1; /* Caller must inspect errno to determine cause. */
    }

    if (redis_ep->reply_len) {
        int32_t rc = mp_decode_fbs(redis_ep->reply_str, redis_ep->reply_len,
            buffer, buffer_length, endpoint, channel_name);
        redis_ep->reply_len = 0;
        if (reply) freeReplyObject(reply);
        return rc;
    }
    if (reply) freeReplyObject(reply);
    return 0;
}


void redis_interrupt(Endpoint* endpoint)
{
    endpoint->stop_request = 1;
}


void redis_disconnect(Endpoint* endpoint)
{
    redis_endpoint_destroy(endpoint);
}
