// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/redispubsub.h>
#include <dse/modelc/adapter/transport/endpoint.h>


#define REDIS_CONNECTION_TIMEOUT 5  /* Double, seconds. */
#define REDIS_SUBSCRIBE_TIMEOUT  10 /* Seconds. */
#define MAX_KEY_SIZE             64
#define ARRAY_SIZE(x)            (sizeof(x) / sizeof(x[0]))
#define UNUSED(x)                ((void)x)


void redispubsub_on_message(
    redisAsyncContext* sub_ctx, void* _reply, void* privdata);


static void redis_endpoint_destroy(Endpoint* endpoint)
{
    if (endpoint && endpoint->private) {
        RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;
        if (redis_ep->ctx) redisFree(redis_ep->ctx);
        if (redis_ep->sub_ctx) redisAsyncFree(redis_ep->sub_ctx);
        if (redis_ep->sub__argv) {
            if (redis_ep->sub__argv[0]) free(redis_ep->sub__argv[0]);
            free(redis_ep->sub__argv);
        }
        if (redis_ep->sub_event_timeout)
            event_free(redis_ep->sub_event_timeout);
        if (redis_ep->sub_event_base) event_base_free(redis_ep->sub_event_base);
        /* Release the endpoint_channels hashmap, this is only a lookup, so
        there is no need to release the contained objects. */
        hashmap_destroy(&redis_ep->endpoint_lookup);
        /* Free any items in the queue. */
        queue_node* n;
        q_traverse(redis_ep->recv_msg_queue, n)
        {
            RedisPubSubMessage* msg = (RedisPubSubMessage*)n->data;
            free(msg->buffer);
        }
        q_free_alt(redis_ep->recv_msg_queue, true);
        /* Release the RedisPubSubEndpoint object. */
        free(endpoint->private);
    }
    if (endpoint) {
        /* Release the endpoint_channels hashmap. */
        char**   keys = hashmap_keys(&endpoint->endpoint_channels);
        uint32_t count = hashmap_number_keys(endpoint->endpoint_channels);
        for (uint32_t i = 0; i < count; i++) {
            RedisPubSubChannel* ch =
                hashmap_get(&endpoint->endpoint_channels, keys[i]);
            if (ch->pub_key) free(ch->pub_key);
            if (ch->sub_key) free(ch->sub_key);
            free(ch);
        }
        hashmap_destroy(&endpoint->endpoint_channels);
        for (uint32_t _ = 0; _ < count; _++)
            free(keys[_]);
        free(keys);
    }
    free(endpoint);
}


Endpoint* redispubsub_connect(const char* path, const char* hostname,
    int32_t port, uint32_t model_uid, bool bus_mode, double recv_timeout)
{
    int       rc;
    /* Endpoint. */
    Endpoint* endpoint = calloc(1, sizeof(Endpoint));
    if (endpoint == NULL) {
        log_error("Endpoint malloc failed!");
        goto error_clean_up;
    }
    endpoint->bus_mode = bus_mode;
    endpoint->create_channel = redispubsub_create_channel;
    endpoint->start = redispubsub_start;
    endpoint->send_fbs = redispubsub_send_fbs;
    endpoint->recv_fbs = redispubsub_recv_fbs;
    endpoint->interrupt = redispubsub_interrupt;
    endpoint->disconnect = redispubsub_disconnect;
    rc = hashmap_init_alt(&endpoint->endpoint_channels, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for endpoint->endpoint_channels!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }

    /* Redis Endpoint. */
    RedisPubSubEndpoint* redis_ep = calloc(1, sizeof(RedisPubSubEndpoint));
    if (redis_ep == NULL) {
        log_error("Redis_ep malloc failed!");
        goto error_clean_up;
    }
    redis_ep->path = path;
    redis_ep->hostname = hostname;
    redis_ep->port = port;
    redis_ep->recv_timeout = recv_timeout;
    rc = hashmap_init_alt(&redis_ep->endpoint_lookup, 32, NULL);
    if (rc) {
        log_error("Hashmap init failed for redis_ep->endpoint_lookup!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    redis_ep->recv_msg_queue = q_init();
    endpoint->private = (void*)redis_ep;

    /* Redis connection - also used for TX direction (pub_key) all channels. */
    struct timeval timeout = { REDIS_CONNECTION_TIMEOUT, 0 };
    log_notice("  Redis Pub/Sub:");
    log_notice("    path: %s", redis_ep->path);
    log_notice("    hostname: %s", redis_ep->hostname);
    log_notice("    port: %d", redis_ep->port);
    if (redis_ep->hostname) {
        redis_ep->ctx = redisConnectWithTimeout(
            redis_ep->hostname, redis_ep->port, timeout);
    } else if (redis_ep->path) {
        redis_ep->ctx = redisConnectUnixWithTimeout(redis_ep->path, timeout);
    } else {
        log_fatal("Redis Pub/Sub connect parameters not complete.");
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

    /* Model UID. */
    redisReply* reply;
    reply = redisCommand(redis_ep->ctx, "CLIENT ID");
    redis_ep->client_id = reply->integer;
    freeReplyObject(reply);
    if (model_uid) {
        endpoint->uid = model_uid;
    } else {
        endpoint->uid = redis_ep->client_id;
    }

    /* Log the Endpoint information. */
    log_notice("  Endpoint: ");
    log_notice("    Model UID: %i", endpoint->uid);
    log_notice("    Client ID: %i", redis_ep->client_id);

    return endpoint;

error_clean_up:
    redis_endpoint_destroy(endpoint);
    return NULL;
}


void redispubsub_disconnect(Endpoint* endpoint)
{
    redis_endpoint_destroy(endpoint);
}


void* redispubsub_create_channel(Endpoint* endpoint, const char* channel_name)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;

    /* Check if the endpoint channel already exists. */
    RedisPubSubChannel* endpoint_channel =
        hashmap_get(&endpoint->endpoint_channels, channel_name);
    if (endpoint_channel) {
        assert(endpoint_channel->channel_name == channel_name);
        return (void*)endpoint_channel;
    }

    /* Create the RedisPubSubChannel object. */
    endpoint_channel = calloc(1, sizeof(RedisPubSubChannel));
    assert(endpoint_channel);
    endpoint_channel->pub_key = calloc(1, MAX_KEY_SIZE);
    endpoint_channel->sub_key = calloc(1, MAX_KEY_SIZE);
    endpoint_channel->channel_name = channel_name;
    if (endpoint->bus_mode) {
        /* BUS Mode == reversed, send on RX (Model reads)... */
        snprintf(endpoint_channel->pub_key, MAX_KEY_SIZE - 1, "bus.ch.%s.rx",
            channel_name);
        snprintf(endpoint_channel->sub_key, MAX_KEY_SIZE - 1, "bus.ch.%s.tx",
            channel_name);
    } else {
        snprintf(endpoint_channel->pub_key, MAX_KEY_SIZE - 1, "bus.ch.%s.tx",
            channel_name);
        snprintf(endpoint_channel->sub_key, MAX_KEY_SIZE - 1, "bus.ch.%s.rx",
            channel_name);
    }

    /* Add to Endpoint->endpoint_channels. */
    if (hashmap_set(&endpoint->endpoint_channels, channel_name,
            endpoint_channel) == NULL) {
        assert(0);
    }
    /* Set the lookup for mapping from PubSub key to endpoint_channel. */
    if (hashmap_set(&redis_ep->endpoint_lookup, endpoint_channel->pub_key,
            endpoint_channel) == NULL) {
        assert(0);
    }
    if (hashmap_set(&redis_ep->endpoint_lookup, endpoint_channel->sub_key,
            endpoint_channel) == NULL) {
        assert(0);
    }

    log_notice("  Pub Key: %s", endpoint_channel->pub_key);
    log_notice("  Sub Key: %s", endpoint_channel->sub_key);

    /* Return the created object, to the caller (which will be an Adapter). */
    return (void*)endpoint_channel;
}


static int  __event_timeout_condition__ = 0;
static void __event_timeout_handler__(
    evutil_socket_t fd, int16_t events, void* arg)
{
    UNUSED(fd);
    UNUSED(events);
    __event_timeout_condition__ = 1;
    event_base_loopbreak(arg);
}


int32_t redispubsub_start(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;

    /* Redis connection - RX direction (sub_key) one for each channel. */
    if (redis_ep->hostname) {
        redis_ep->sub_ctx =
            redisAsyncConnect(redis_ep->hostname, redis_ep->port);
    } else if (redis_ep->path) {
        redis_ep->sub_ctx = redisAsyncConnectUnix(redis_ep->path);
    } else {
        log_fatal("Redis Pub/Sub connect parameters not complete.");
    }
    redisAsyncContext* _sub_ctx = (redisAsyncContext*)redis_ep->sub_ctx;
    if (_sub_ctx == NULL || _sub_ctx->err) {
        if (_sub_ctx) {
            log_notice("Connection error: %s", _sub_ctx->errstr);
        } else {
            log_notice("Connection error: can't allocate Redis context");
        }
        log_error("Redis connect failed!");
        goto error_clean_up;
    }
    signal(SIGPIPE, SIG_IGN);
    redis_ep->sub_event_base = event_base_new();
    redisLibeventAttach(redis_ep->sub_ctx, redis_ep->sub_event_base);
    /* Build the SUB command. */
    char**   keys = hashmap_keys(&endpoint->endpoint_channels);
    uint32_t count = hashmap_number_keys(endpoint->endpoint_channels);
    log_trace("Endpoint channel keys : %u", count);
    redis_ep->sub__argc = count + 1;
    redis_ep->sub__argv = calloc(redis_ep->sub__argc, sizeof(char*));
    redis_ep->sub__argv[0] = calloc(1, MAX_KEY_SIZE);
    snprintf(redis_ep->sub__argv[0], MAX_KEY_SIZE - 1, "SUBSCRIBE");
    for (uint32_t i = 0; i < count; i++) {
        RedisPubSubChannel* ch =
            hashmap_get(&endpoint->endpoint_channels, keys[i]);
        redis_ep->sub__argv[i + 1] = ch->sub_key;
    }
    for (uint32_t _ = 0; _ < count; _++)
        free(keys[_]);
    free(keys);
    /* Run the SUB command. */
    log_trace("Subscribe Command : ");
    for (int i = 0; i < redis_ep->sub__argc; i++)
        log_trace("    %s", redis_ep->sub__argv[i]);
    redis_ep->sub_active = false;
    redisAsyncCommandArgv(redis_ep->sub_ctx, redispubsub_on_message,
        (void*)endpoint, redis_ep->sub__argc, (const char**)redis_ep->sub__argv,
        0);

    /* Wait for the PubSub to startup.*/
    struct timeval timeout = { REDIS_SUBSCRIBE_TIMEOUT, 0 };
    redis_ep->sub_event_timeout = event_new(redis_ep->sub_event_base, -1, 0,
        &__event_timeout_handler__, redis_ep->sub_event_base);
    if (!redis_ep->sub_event_timeout) {
        log_error("event_new() failed!");
        goto error_clean_up;
    }
    event_add(redis_ep->sub_event_timeout, &timeout);
    event_base_dispatch(redis_ep->sub_event_base);
    if (!event_base_got_break(redis_ep->sub_event_base)) {
        /* event_base_break() was not called, so no event/message was received.
         */
        log_trace("redispubsub_recv_fbs: no message (timeout)");
    }
    if (!redis_ep->sub_active) {
        log_error("Redis SUBSCRIBE did not activate!");
    }
    event_del(redis_ep->sub_event_timeout);
    event_free(redis_ep->sub_event_timeout);

    /* Start the timeout event. */
    struct timeval timeout_1s = { 1, 0 };
    redis_ep->sub_event_timeout = event_new(redis_ep->sub_event_base, -1,
        EV_PERSIST, &__event_timeout_handler__, redis_ep->sub_event_base);
    if (!redis_ep->sub_event_timeout) {
        log_error("event_new() failed!");
        goto error_clean_up;
    }
    event_add(redis_ep->sub_event_timeout, &timeout_1s);

    return 0;

error_clean_up:
    redis_endpoint_destroy(endpoint);
    return 1;
}


int32_t redispubsub_send_fbs(Endpoint* endpoint, void* endpoint_channel,
    void* buffer, uint32_t buffer_length, uint32_t model_uid)
{
    UNUSED(model_uid);
    assert(endpoint);
    assert(endpoint->private);
    assert(endpoint_channel);

    RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;
    assert(redis_ep->ctx);
    RedisPubSubChannel* ch = (RedisPubSubChannel*)endpoint_channel;

    /** Models first connecting to a SimBus may send messages before the
     *  SimBus has started. As a result, messages are lost. This is a
     *  condition we only expect at startup of Models - in that case a
     *  retry is attempted (according to the general receive timeout).
     */
    int retry_count = endpoint->bus_mode ? 1 : (int)redis_ep->recv_timeout;
    while (retry_count--) {
        redisReply* reply;
        reply = redisCommand(redis_ep->ctx, "PUBLISH %s %b", ch->pub_key,
            buffer, (size_t)buffer_length);
        log_trace("redispubsub_send_fbs: message sent");
        log_trace("redispubsub_send_fbs:     channel=%s", ch->pub_key);
        log_trace("redispubsub_send_fbs:     clients=%lld", reply->integer);
        if (reply->integer == 0) {
            log_notice("redispubsub_send_fbs: no clients received message!"
                       " channel=%s",
                ch->pub_key);
            freeReplyObject(reply);

            if (endpoint->stop_request) {
                errno = ECANCELED;
                return -1;
            }
            sleep(1);
        } else {
            freeReplyObject(reply);
            break;
        }
    }

    return 0;
}


int32_t redispubsub_recv_fbs(Endpoint* endpoint, const char** channel_name,
    uint8_t** buffer, uint32_t* buffer_length)
{
    assert(endpoint);
    assert(endpoint->private);
    assert(channel_name);

    RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;
    assert(redis_ep->ctx);
    assert(redis_ep->sub_ctx);
    static int event_timeout_counter = 0;

    /* Wait for a message, but only if the queue is empty. */
    if (q_num_elements(redis_ep->recv_msg_queue) == 0) {
        /* Wait for a message. */
        event_timeout_counter = 0;
        while (event_timeout_counter++ < (int32_t)redis_ep->recv_timeout) {
            event_base_dispatch(redis_ep->sub_event_base);
            if (event_base_got_break(redis_ep->sub_event_base)) {
                if (__event_timeout_condition__) {
                    /* Event timeout. */
                    __event_timeout_condition__ = 0;
                    continue;
                } else {
                    /* A message arrived. */
                    break;
                }
            }
            /* The following condition should not occur, interested if it does.
             */
            log_error("Unexpected condition; event_base_dispatch() with no "
                      "indicators.");
        }
        if (event_timeout_counter >= (int32_t)redis_ep->recv_timeout) {
            log_trace("redispubsub_recv_fbs: no message (timeout)");
            errno = ETIME;
            return -1; /* Caller must inspect errno to determine cause. */
        }
    }
    /* Take the message and return to caller. */
    RedisPubSubMessage* msg = q_pop(redis_ep->recv_msg_queue);
    if (msg == NULL) {
        /* No message, queue is empty. */
        *channel_name = NULL;
        return 0; /* Indicate that no message was received. */
    }
    /* Marshal data to caller. */
    if (msg->length > *buffer_length) {
        /* Prepare the buffer, resize if necessary. */
        *buffer = realloc(*buffer, msg->length);
        if (*buffer == NULL) {
            log_error("Malloc failed!");
        }
        *buffer_length = (size_t)msg->length;
    }
    memset(*buffer, 0, *buffer_length);
    memcpy(*buffer, msg->buffer, msg->length);
    *channel_name = msg->endpoint_channel->channel_name;
    int32_t rc = (uint32_t)msg->length;
    free(msg->buffer);
    free(msg);
    /* Return the buffer length (+ve) as indicator of success. */
    return rc;
}


void redispubsub_on_message(
    redisAsyncContext* sub_ctx, void* _reply, void* privdata)
{
    assert(privdata);
    assert(sub_ctx);
    if (_reply == NULL) return;
    redisReply*          reply = _reply;
    Endpoint*            endpoint = (Endpoint*)privdata;
    RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;
    assert(redis_ep->ctx);
    assert(redis_ep->sub_ctx);

    /* Check the message. */
    if (reply->type != REDIS_REPLY_ARRAY && reply->elements >= 2) {
        log_trace("redispubsub_on_message: bad message");
        return;
    }
    if (strcmp("message", reply->element[0]->str) == 0) {
        /* Message is processed below. Indicate the SUBSCRIBE is activated. */
        redis_ep->sub_active = true;
    } else {
        if (!redis_ep->sub_active) {
            /* Caller is waiting for the SUBSCRIBE to activate. */
            redis_ep->sub_active = true;
            /* Indicate that a message arrived, this will halt the event
             * handler. */
            event_base_loopbreak(redis_ep->sub_event_base);
            return;
        }
        /* The event handler should continue until the next message arrives. */
        log_trace("redispubsub_on_message: non-message");
        return;
    }

    /* Process the message: channel in [1] and FBS in [2]. */
    log_trace("redispubsub_on_message: message arrived");
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
        log_trace(
            "redispubsub_on_message:     channel=%s", reply->element[1]->str);
        log_trace("redispubsub_on_message:     length=%d",
            (int32_t)reply->element[2]->len);
        /* Copy the FBS Message into the queue. */
        RedisPubSubMessage* msg = calloc(1, sizeof(RedisPubSubMessage));
        msg->length = reply->element[2]->len;
        msg->buffer = malloc(msg->length);
        memcpy(msg->buffer, reply->element[2]->str, msg->length);
        msg->endpoint_channel =
            hashmap_get(&redis_ep->endpoint_lookup, reply->element[1]->str);
        /* Push the message to the queue. */
        q_push(redis_ep->recv_msg_queue, msg);
        /* Indicate that a message arrived, this will halt the event handler. */
        event_base_loopbreak(redis_ep->sub_event_base);
        return;
    } else {
        log_trace("redispubsub_on_message: bad message layout (type or element "
                  "count)");
        log_trace("Wrong reply type (%d) or elements (%lu) -> Timeout !?!",
            reply->type, reply->elements);
    }

    /* The event handler should continue until the next message arrives. */
    return;
}


void redispubsub_interrupt(Endpoint* endpoint)
{
    assert(endpoint);
    endpoint->stop_request = 1;
    RedisPubSubEndpoint* redis_ep = (RedisPubSubEndpoint*)endpoint->private;
    event_base_loopbreak(redis_ep->sub_event_base);
}
