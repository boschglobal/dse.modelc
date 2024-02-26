// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <msgpack.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/mq.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>


#define MQ_DESC_KEY_LEN 12


static int __stop_request = 0;

static const char* __scheme_table__[] = {
    "posix",
    "namedpipe",
};


const char* mq_scheme(MqScheme scheme)
{
    if (scheme < __MQ_SCHEME_COUNT__) {
        return __scheme_table__[scheme];
    }
    return NULL;
}


void mq_endpoint_destroy(Endpoint* endpoint)
{
    if (endpoint && endpoint->private) {
        MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;
        if (endpoint->disconnect) endpoint->disconnect(endpoint);

        /* Release the mq_ep->push_hash hashmap. */
        char**   keys = hashmap_keys(&mq_ep->push_hash);
        uint32_t count = hashmap_number_keys(mq_ep->push_hash);
        for (uint32_t i = 0; i < count; i++) {
            MqDesc* _ = hashmap_get(&mq_ep->push_hash, keys[i]);
            free(_);
        }
        hashmap_destroy(&mq_ep->push_hash);
        for (uint32_t _ = 0; _ < count; _++)
            free(keys[_]);
        free(keys);
        /* Release the MqEndpoint object. */
        hashmap_destroy(&mq_ep->push_hash);
        free(endpoint->private);
    }
    if (endpoint) {
        hashmap_destroy(&endpoint->endpoint_channels);
    }
    free(endpoint);
}


Endpoint* mq_connect(
    const char* uri, uint32_t model_uid, bool bus_mode, double recv_timeout)
{
    int       rc;
    Endpoint* endpoint = calloc(1, sizeof(Endpoint));
    if (endpoint == NULL) {
        log_error("Endpoint malloc failed!");
        goto error_clean_up;
    }
    rc = hashmap_init_alt(&endpoint->endpoint_channels, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for endpoint->endpoint_channels!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    endpoint->bus_mode = bus_mode;
    endpoint->uid = model_uid;

    /* MQ Endpoint. */
    MqEndpoint* mq_ep = calloc(1, sizeof(MqEndpoint));
    if (mq_ep == NULL) {
        log_error("MqEndpoint malloc failed!");
        goto error_clean_up;
    }
    endpoint->private = (void*)mq_ep;
    mq_ep->recv_timeout = recv_timeout;
    rc = hashmap_init_alt(&mq_ep->push_hash, 16, NULL);
    if (rc) {
        log_error("Hashmap init failed for mq_ep->push_hash!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }

    /**
     * Parse the URI for MQ transport and filename stem. The filename stem is
     * used to construct the message queue files so that each simulation can
     * have a different set of files (differentiated by the stem part).
     */
    char _uri[MQ_MAX_EP_LEN];
    strncpy(_uri, uri, MQ_MAX_EP_LEN - 1);
    char* needle = strstr(_uri, URI_SCHEME_DELIM);
    if (needle == NULL) {
        log_error("URI format wrong: <transport>:///<stem> (%s)", uri);
        log_fatal("Bad transport in URI");
    }
    _uri[needle - _uri] = '\0';
    const char* uri_transport = _uri;
    log_trace("uri_transport: : %s :", uri_transport);
    const char* uri_stem = needle + strlen(URI_SCHEME_DELIM);
    if (*uri_stem == '\0') {
        log_error("URI format wrong: <transport>:///<stem> (%s)", uri);
        log_fatal("Bad stem in URI");
    }
    /* Set the MQ Endpoint stem (which is used elsewhere). */
    mq_ep->stem = strdup(uri_stem);
    log_trace("uri_stem: : %s :", mq_ep->stem);

    /* Configure the Endpoint. */
    if (endpoint->bus_mode) {
        /* SimBus. Push EP are allocated/assigned in Hash. */
        snprintf(
            mq_ep->pull.endpoint, MQ_MAX_EP_LEN, "%s.dse.simbus", mq_ep->stem);
        /* Log the Endpoint information. */
        log_notice("  Endpoint: ");
        log_notice("    Model UID: %i", endpoint->uid);
        log_notice("    Pull Endpoint: %s", mq_ep->pull.endpoint);
    } else {
        /* Model. */
        snprintf(
            mq_ep->push.endpoint, MQ_MAX_EP_LEN, "%s.dse.simbus", mq_ep->stem);
        snprintf(mq_ep->pull.endpoint, MQ_MAX_EP_LEN, "%s.dse.model.%d",
            mq_ep->stem, endpoint->uid);
        /* Log the Endpoint information. */
        log_notice("  Endpoint: ");
        log_notice("    Model UID: %i", endpoint->uid);
        log_notice("    Push Endpoint: %s", mq_ep->push.endpoint);
        log_notice("    Pull Endpoint: %s", mq_ep->pull.endpoint);
    }
    endpoint->create_channel = mq_create_channel;
    endpoint->start = mq_start;
    endpoint->send_fbs = mq_send_fbs;
    endpoint->recv_fbs = mq_recv_fbs;
    endpoint->interrupt = mq_interrupt;
    endpoint->disconnect = mq_disconnect;
#ifdef _WIN32
    if (strcmp(uri_transport, mq_scheme(MQ_SCHEME_NAMEDPIPE)) == 0) {
        /* Setup a Named Pipe MQ  Endpoint. */
        log_fatal("Endpoint not implemented");
        mq_ep->scheme = mq_scheme(MQ_SCHEME_NAMEDPIPE);
        // mq_namedpipe_configure(endpoint);
        return endpoint;
    }
#else
    if (strcmp(uri_transport, mq_scheme(MQ_SCHEME_POSIX)) == 0) {
        /* Setup a POSIX MQ  Endpoint. */
        mq_ep->scheme = mq_scheme(MQ_SCHEME_POSIX);
        mq_posix_configure(endpoint);
        return endpoint;
    }
#endif

    log_error("Endpoint scheme unknown (%s)!", uri);

error_clean_up:
    mq_endpoint_destroy(endpoint);
    return NULL;
}

MqDesc* mq_model_push_desc(Endpoint* endpoint, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    /* Models always push to SimBus. */
    if (!endpoint->bus_mode) return &mq_ep->push;

    /* Locate the push MQ from the hash. */
    MqDesc* push_desc = NULL;
    char    hash_key[MQ_DESC_KEY_LEN];
    snprintf(hash_key, MQ_DESC_KEY_LEN - 1, "%d", model_uid);
    push_desc = hashmap_get(&mq_ep->push_hash, hash_key);
    if (push_desc) return push_desc;

    /* Create a new entry (will be a Model Push). */
    push_desc = calloc(1, sizeof(MqDesc));
    assert(push_desc);
    snprintf(push_desc->endpoint, MQ_MAX_EP_LEN, "%s.dse.model.%d", mq_ep->stem,
        model_uid);
    if (mq_ep->mq_open) mq_ep->mq_open(push_desc, MQ_KIND_MODEL, MQ_MODE_PUSH);
    hashmap_set(&mq_ep->push_hash, hash_key, push_desc);

    return push_desc;
}


void* mq_create_channel(Endpoint* endpoint, const char* channel_name)
{
    assert(endpoint);
    assert(endpoint->private);

    /* Maintain a list of channel_names. There is no associated metadata
       so only store the channel name pointer. */
    if (hashmap_get(&endpoint->endpoint_channels, channel_name) == NULL) {
        hashmap_set(
            &endpoint->endpoint_channels, channel_name, (void*)channel_name);
        log_notice("    Endpoint Channel : %s\n", channel_name);
    }

    /* Return the created object, to the caller (which will be an Adapter). */
    return (void*)channel_name;
}


static void _close_all_hash_endpoints(MqEndpoint* mq_ep)
{
    char**   keys = hashmap_keys(&mq_ep->push_hash);
    uint32_t count = hashmap_number_keys(mq_ep->push_hash);
    for (uint32_t i = 0; i < count; i++) {
        MqDesc* mq_desc = hashmap_get(&mq_ep->push_hash, keys[i]);
        log_debug("MQ close: endpoint:%s", mq_desc->endpoint);
        if (mq_ep->mq_close) mq_ep->mq_close(mq_desc);
    }
    for (uint32_t _ = 0; _ < count; _++)
        free(keys[_]);
    free(keys);
}


static MqDesc* _get_push_mq(Endpoint* endpoint, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);

    MqDesc* mq_desc = mq_model_push_desc(endpoint, model_uid);
    assert(mq_desc);
    return mq_desc;
}


int32_t mq_start(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    if (mq_ep->mq_open == NULL) log_fatal("MQ not configured");

    /* Unlink Pull MQs (only), this will force a recreation of MQs to pickup
       any changed OS settings.*/
    if (mq_ep->mq_unlink) mq_ep->mq_unlink(&mq_ep->pull);

    /* Bus Mode - unlink all MQ's to ensure they are created with expected
       attributes. */
    if (endpoint->bus_mode) {
        /* PUSH are Models, handled by hash. */
        /* PULL is SimBus.*/
        mq_ep->mq_open(&mq_ep->pull, MQ_KIND_SIMBUS, MQ_MODE_PULL);
        log_debug("MQ opened: endpoint:%s (%d)", mq_ep->pull.endpoint);
    } else {
        /* PUSH is SimBus. */
        mq_ep->mq_open(&mq_ep->push, MQ_KIND_SIMBUS, MQ_MODE_PUSH);
        /* PULL is Model.*/
        mq_ep->mq_open(&mq_ep->pull, MQ_KIND_MODEL, MQ_MODE_PULL);

        log_debug("MQ opened: endpoint:%s", mq_ep->push.endpoint);
        log_debug("MQ opened: endpoint:%s", mq_ep->pull.endpoint);
    }

    return 0;
}


void mq_interrupt(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    __stop_request = 1;
    if (mq_ep->mq_interrupt) mq_ep->mq_interrupt();
}


void mq_disconnect(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    /* Close the MQs. */
    if (mq_ep->mq_close) {
        _close_all_hash_endpoints(mq_ep);

        mq_ep->mq_close(&mq_ep->push);
        mq_ep->mq_close(&mq_ep->pull);
        log_debug("MQ close: endpoint:%s", mq_ep->push.endpoint);
        log_debug("MQ close: endpoint:%s", mq_ep->pull.endpoint);
    }

    /* Only unlink the Pull MQ. */
    if (mq_ep->mq_unlink) mq_ep->mq_unlink(&mq_ep->pull);
}


int32_t mq_send_fbs(Endpoint* endpoint, void* endpoint_channel, void* buffer,
    uint32_t buffer_length, uint32_t model_uid)
{
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;
    int         rc = 0;

    if (mq_ep->mq_send == NULL) log_fatal("MQ not configured");

    const char*     channel_name = endpoint_channel;
    msgpack_sbuffer sbuf = mp_encode_fbs(buffer, buffer_length, channel_name);
    if (sbuf.size > MQ_MAX_MSGSIZE) {
        log_fatal("Message size (%d) exceeds MQ_MAX_MSGSIZE (%d)", sbuf.size,
            MQ_MAX_MSGSIZE);
    }

    /* Send the message/datagram. */
    if (endpoint_channel) {
        /* Sending a Channel Message. */
        MqDesc* push_desc = _get_push_mq(endpoint, model_uid);
        assert(push_desc);
        log_trace(
            "MQ send: endpoint:%s, length: %d", push_desc->endpoint, sbuf.size);
        rc = mq_ep->mq_send(push_desc, (char*)sbuf.data, (size_t)sbuf.size);
        if (rc != 0) {
            if (errno == 0) errno = ENODATA;
            log_error("mq_send failed!");
        }
    } else {
        if (!endpoint->bus_mode) {
            MqDesc* push_desc = _get_push_mq(endpoint, model_uid);
            assert(push_desc);
            log_trace("MQ send: endpoint:%s, length: %d", push_desc->endpoint,
                sbuf.size);
            rc = mq_ep->mq_send(push_desc, (char*)sbuf.data, (size_t)sbuf.size);
            if (rc != 0) {
                if (errno == 0) errno = ENODATA;
                log_error("mq_send failed!");
            }

        } else {
            /* Sending a Notify Message to each model. */
            char**   keys = hashmap_keys(&mq_ep->push_hash);
            uint32_t count = hashmap_number_keys(mq_ep->push_hash);
            for (uint32_t i = 0; i < count; i++) {
                MqDesc* push_desc = hashmap_get(&mq_ep->push_hash, keys[i]);
                rc = mq_ep->mq_send(
                    push_desc, (char*)sbuf.data, (size_t)sbuf.size);
                if (rc != 0) {
                    if (errno == 0) errno = ENODATA;
                    log_error("mq_send failed!");
                }
            }
            for (uint32_t _ = 0; _ < count; _++)
                free(keys[_]);
            free(keys);
        }
    }

    /* Release the encoded datagram. */
    msgpack_sbuffer_destroy(&sbuf);

    return rc;
}


int32_t mq_recv_fbs(Endpoint* endpoint, const char** channel_name,
    uint8_t** buffer, uint32_t* buffer_length)
{
    assert(endpoint);
    assert(endpoint->private);
    assert(channel_name);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    if (mq_ep->mq_recv == NULL) log_fatal("MQ not configured");

    int             msg_len;
    static char     msg[MQ_MAX_MSGSIZE];
    struct timespec tm;
    int             timeout_counter = (int32_t)mq_ep->recv_timeout + 1;

    *channel_name = NULL; /* Set the default return condition. */

    while (--timeout_counter) {
        if (__stop_request) {
            /* Request to exit. */
            return 0;
        }
        /* Timed receive for 1 second (at a time). Normally no performance hit
         * in doing this loop as we expect messages on a shorter time frame.
         */
        clock_gettime(CLOCK_REALTIME, &tm);
        tm.tv_sec += 1;
        msg_len = mq_ep->mq_recv(&mq_ep->pull, msg, MQ_MAX_MSGSIZE, &tm);
        if (msg_len < 0) {
            if (errno == 0) errno = ENODATA;
            if (errno == ETIMEDOUT) {
                /* Timeout on the call, next loop. */
                continue;
            }
            log_fatal("mq_timedreceive failed!");
            return -1; /* -ve number indicates failure to receive. */
        }
        log_trace(
            "MQ recv: endpoint:%s, length: %d", mq_ep->pull.endpoint, msg_len);
        break;
    }
    if (timeout_counter == 0) {
        log_trace("mq_posix_recv_fbs: no message (timeout)");
        errno = ETIME;
        return -1; /* Caller must inspect errno to determine cause. */
    }
    if (msg_len <= 0) {
        /* No message, queue is empty. */
        return 0; /* Indicate that no message was received. */
    }

    return mp_decode_fbs(
        msg, msg_len, buffer, buffer_length, endpoint, channel_name);
}
