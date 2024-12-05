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
        /* Pull args. */
        free(redis_ep->pull_argv);
        free(redis_ep->pull_argvlen);
        /* Release the hashmaps. */
        hashmap_destroy_ext(&redis_ep->push_hash, NULL, NULL);
        hashmap_destroy_ext(&redis_ep->notify_push_hash, NULL, NULL);
        hashmap_destroy_ext(&redis_ep->pull_hash, NULL, NULL);
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

static void _update_pull_cmd(RedisEndpoint* redis_ep);


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
    endpoint->register_notify_uid = redis_register_notify_uid;
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
    rc = hashmap_init_alt(&redis_ep->notify_push_hash, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for redis_ep->notify_push_hash!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    rc = hashmap_init_alt(&redis_ep->pull_hash, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for redis_ep->pull_hash!");
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
    if (reply == NULL) {
        log_error("Connection lost: redisCommand returned NULL object");
        goto error_clean_up;
    }
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
    if (reply == NULL) {
        log_error("Connection lost: redisCommand returned NULL object");
        goto error_clean_up;
    }
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
        RedisKeyDesc* pull_desc = calloc(1, sizeof(RedisKeyDesc));
        assert(pull_desc);
        snprintf(pull_desc->endpoint, MAX_KEY_SIZE, "dse.simbus");
        hashmap_set(&redis_ep->pull_hash, "simbus", pull_desc);
        _update_pull_cmd(redis_ep);
        /* Log the Endpoint information. */
        log_notice("  Endpoint: ");
        log_notice("    Model UID: %i", endpoint->uid);
        log_notice("    Pull Endpoint: %s", redis_ep->pull.endpoint);
    } else {
        /* Model. */
        snprintf(redis_ep->push.endpoint, MAX_KEY_SIZE, "dse.simbus");
        /* Log the Endpoint information. */
        log_notice("  Endpoint: ");
        log_notice("    Model UID: %i", endpoint->uid);
        log_notice("    Push Endpoint: %s", redis_ep->push.endpoint);
        _update_pull_cmd(redis_ep);
    }

    return endpoint;

error_clean_up:
    redis_endpoint_destroy(endpoint);
    return NULL;
}


void redis_register_notify_uid(Endpoint* endpoint, uint32_t notify_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;

    if (endpoint->bus_mode && notify_uid) {
        /* Locate the push key from the hash. */
        RedisKeyDesc* push_desc = NULL;
        char          hash_key[DESC_KEY_LEN];
        snprintf(hash_key, DESC_KEY_LEN - 1, "%d", notify_uid);
        push_desc = hashmap_get(&redis_ep->notify_push_hash, hash_key);
        if (push_desc) return;

        /* Create a new entry (will be a Model Push). */
        push_desc = calloc(1, sizeof(RedisKeyDesc));
        assert(push_desc);
        snprintf(push_desc->endpoint, MAX_KEY_SIZE, "dse.model.%d", notify_uid);
        hashmap_set(&redis_ep->notify_push_hash, hash_key, push_desc);
    }
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

static int _add_pull_key(void* _key, void* _endpoint)
{
    char*          key = _key;
    RedisEndpoint* redis_ep = _endpoint;

    redis_ep->pull_argv[redis_ep->pull_argc] = key;
    redis_ep->pull_argvlen[redis_ep->pull_argc] = strlen(key);
    redis_ep->pull_argc += 1;

    return 0;
}

static void _update_pull_cmd(RedisEndpoint* redis_ep)
{
    redis_ep->pull.endpoint[0] = '\0';
    redis_ep->pull_argc = 0;
    free(redis_ep->pull_argv);
    free(redis_ep->pull_argvlen);

    size_t ep_count = hashmap_number_keys(redis_ep->pull_hash);
    redis_ep->pull_argv = calloc((2 + ep_count), sizeof(*redis_ep->pull_argv));
    redis_ep->pull_argvlen =
        calloc((2 + ep_count), sizeof(*redis_ep->pull_argvlen));

    /* PULL Command. */
    redis_ep->pull_argv[0] = (char*)"BRPOP";
    redis_ep->pull_argvlen[0] = strlen("BRPOP");
    redis_ep->pull_argc += 1;
    /* PULL Keys. */
    hashmap_iterator(&redis_ep->pull_hash, _add_pull_key, false, redis_ep);
    /* PULL Timeout. */
    if (redis_ep->major_ver >= 6) {
        redis_ep->pull_argv[redis_ep->pull_argc] = (char*)"1.0";
        redis_ep->pull_argvlen[redis_ep->pull_argc] = strlen("1.0");
    } else {
        redis_ep->pull_argv[redis_ep->pull_argc] = (char*)"1";
        redis_ep->pull_argvlen[redis_ep->pull_argc] = strlen("1");
    }
    redis_ep->pull_argc += 1;
}

static void _update_pull_key(Endpoint* endpoint, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;

    /* Model pull only. */
    if (endpoint->bus_mode) return;
    if (model_uid == 0) return;

    /* Locate the pull key from the hash. */
    RedisKeyDesc* pull_desc = NULL;
    char          hash_key[DESC_KEY_LEN];
    snprintf(hash_key, DESC_KEY_LEN - 1, "%d", model_uid);
    pull_desc = hashmap_get(&redis_ep->pull_hash, hash_key);
    if (pull_desc) return;

    /* Add a new entry, and update the pull endpoint command. */
    pull_desc = calloc(1, sizeof(RedisKeyDesc));
    assert(pull_desc);
    snprintf(pull_desc->endpoint, MAX_KEY_SIZE, "dse.model.%d", model_uid);
    hashmap_set(&redis_ep->pull_hash, hash_key, pull_desc);
    _update_pull_cmd(redis_ep);

    log_notice("    Pull Endpoint: %s", pull_desc->endpoint);
}


int32_t redis_send_fbs(Endpoint* endpoint, void* endpoint_channel, void* buffer,
    uint32_t buffer_length, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisEndpoint* redis_ep = (RedisEndpoint*)endpoint->private;
    int            rc = 0;

    _update_pull_key(endpoint, model_uid);

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
            HashMap* push_hash;
            if (model_uid == 0) {
                push_hash = &redis_ep->notify_push_hash;
            } else {
                push_hash = &redis_ep->push_hash;
            }

            /* Sending a Notify Message to each model. */
            char**   keys = hashmap_keys(push_hash);
            uint32_t count = hashmap_number_keys((*push_hash));
            for (uint32_t i = 0; i < count; i++) {
                RedisKeyDesc* push_desc = hashmap_get(push_hash, keys[i]);
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

    if (reply && reply->type == REDIS_REPLY_ERROR) {
        log_debug("REDIS_REPLY_ERROR : %s", reply->str);
    }
    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
        redis_ep->reply_len = reply->element[1]->len;
        if (redis_ep->reply_len > redis_ep->reply_alloc_size) {
            free(redis_ep->reply_str);
            redis_ep->reply_alloc_size = redis_ep->reply_len;
            redis_ep->reply_str = malloc(redis_ep->reply_alloc_size);
        }
        memcpy(
            redis_ep->reply_str, reply->element[1]->str, redis_ep->reply_len);
    } else {
        if (reply && reply->type == REDIS_REPLY_NIL) errno = ETIMEDOUT;
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
        log_trace("Redis recv: endpoint:%s", redis_ep->pull.endpoint);
        if (redis_ep->async_ctx) {
            redisAsyncCommandArgv(redis_ep->async_ctx, _redis_async_brpop,
                redis_ep, redis_ep->pull_argc,
                (const char**)redis_ep->pull_argv,
                (const size_t*)redis_ep->pull_argvlen);
            event_base_dispatch(redis_ep->base);
        } else {
            reply = redisCommandArgv(redis_ep->ctx, redis_ep->pull_argc,
                (const char**)redis_ep->pull_argv,
                (const size_t*)redis_ep->pull_argvlen);
            if (reply && reply->type == REDIS_REPLY_ERROR) {
                log_debug("REDIS_REPLY_ERROR : %s", reply->str);
            }
            if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
                redis_ep->reply_len = reply->element[1]->len;
                redis_ep->reply_str = reply->element[1]->str;
            } else {
                if (reply && reply->type == REDIS_REPLY_NIL) errno = ETIMEDOUT;
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
