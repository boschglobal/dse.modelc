// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/adapter/transport/stream.h>

#ifdef _WIN32
#include <dse/modelc/adapter/transport/stream_win.h>
#else
#include <dse/modelc/adapter/transport/stream_linux.h>
#endif


#define UNUSED(x)                    ((void)x)

#define SOCKET_CONNECTION_TIMEOUT_MS 5000U
#define STREAM_MAX_HOST_LENGTH       255
#define STREAM_DEFAULT_TCP_PORT      5042
#define UNIX_URI_PREFIX              "unix://"
#define TCP_URI_PREFIX               "tcp://"


static Endpoint* _create_driver(const char* uri)
{
    UNUSED(uri);

    Endpoint* endpoint = calloc(1, sizeof(Endpoint));
    if (endpoint == NULL) {
        log_fatal("Endpoint malloc failed");
    }

    endpoint->create_channel = stream_create_channel;
    endpoint->start = stream_posix_start;
    endpoint->send_fbs = stream_posix_send_fbs;
    endpoint->recv_fbs = stream_posix_recv_fbs;
    endpoint->interrupt = stream_interrupt;
    endpoint->disconnect = stream_posix_disconnect;

    return endpoint;
}


static int32_t _parse_unix_uri(
    const char* uri, struct sockaddr_storage* storage, socklen_t* addr_len)
{
    struct sockaddr_un* addr = (struct sockaddr_un*)storage;

    *storage = (struct sockaddr_storage){ 0 };
    *addr_len = 0;

    const char* path = uri + strlen(UNIX_URI_PREFIX);
    if (*path == '\0') {
        log_error("Unix stream URI has no socket path: %s", uri);
        return -EINVAL;
    }
    size_t path_length = strlen(path) + 1;
    if (path_length > sizeof(addr->sun_path)) {
        log_error("Unix socket path is too long: %s", path);
        return -ENAMETOOLONG;
    }

    addr->sun_family = AF_UNIX;
    memcpy(addr->sun_path, path, path_length);
    *addr_len =
        (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_length);

    return 0;
}


static int32_t _parse_tcp_authority(const char* authority, char* host,
    size_t host_size, char* port, size_t port_size)
{
    if (*authority == '\0') return -EINVAL;

    snprintf(port, port_size, "%u", STREAM_DEFAULT_TCP_PORT);

    /* IPv6 */
    if (authority[0] == '[') {
        /* Bracketed IPv6: tcp://[::1]:5000 */
        const char* end = strchr(authority, ']');
        if (end == NULL) {
            return -EINVAL;
        }

        size_t host_length = (size_t)(end - authority - 1);
        if (host_length == 0 || host_length >= host_size) {
            return -ENAMETOOLONG;
        }

        memcpy(host, authority + 1, host_length);
        host[host_length] = '\0';

        if (end[1] == ':') {
            if (end[2] == '\0' || strlen(end + 2) >= port_size) {
                return -EINVAL;
            }
            memcpy(port, end + 2, strlen(end + 2) + 1);
        } else if (end[1] != '\0') {
            return -EINVAL;
        }
        return 0;
    }

    const char* first_colon = strchr(authority, ':');
    const char* last_colon = strrchr(authority, ':');

    if (first_colon != NULL && first_colon != last_colon) {
        // An IPv6 address containing ':' must use brackets.
        return -EINVAL;
    }

    /* IPv4 */
    size_t host_length;
    if (last_colon != NULL) {
        host_length = (size_t)(last_colon - authority);
        const char* port_value = last_colon + 1;
        if (*port_value == '\0' || strlen(port_value) >= port_size) {
            return -EINVAL;
        }
        memcpy(port, port_value, strlen(port_value) + 1);
    } else {
        host_length = strlen(authority);
    }
    if (host_length == 0 || host_length >= host_size) {
        return -ENAMETOOLONG;
    }
    memcpy(host, authority, host_length);
    host[host_length] = '\0';
    return 0;
}


static int32_t _parse_tcp_uri(
    const char* uri, struct sockaddr_storage* storage, socklen_t* addr_len)
{
    char host[STREAM_MAX_HOST_LENGTH];
    char port[16];

    const char* authority = uri + strlen(TCP_URI_PREFIX);
    int32_t     rc =
        _parse_tcp_authority(authority, host, sizeof(host), port, sizeof(port));
    if (rc < 0) {
        log_error("Invalid TCP stream URI: %s", uri);
        return rc;
    }

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };
    struct addrinfo* results = NULL;
    int              gai_rc = getaddrinfo(host, port, &hints, &results);
    if (gai_rc != 0) {
        log_error("Unable to resolve TCP stream address %s:%s: %s", host, port,
            gai_strerror(gai_rc));
        return stream_getaddrinfo_error(gai_rc);
    }

    /* Select the first address available. */
    int32_t result = -EADDRNOTAVAIL;
    for (struct addrinfo* ai = results; ai != NULL; ai = ai->ai_next) {
        if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) {
            continue;
        }
        if (ai->ai_addrlen > sizeof(*storage)) {
            continue;
        }

        memset(storage, 0, sizeof(*storage));
        memcpy(storage, ai->ai_addr, ai->ai_addrlen);
        *addr_len = (socklen_t)ai->ai_addrlen;
        result = 0;
        break;
    }
    freeaddrinfo(results);

    if (result < 0) {
        log_error("No usable TCP stream address found for %s:%s", host, port);
    }
    return result;
}


static int32_t _parse_stream_uri(const char* uri, StreamEndpoint* stream_ep)
{
    if (uri == NULL) return -EINVAL;

    if (strncmp(uri, UNIX_URI_PREFIX, strlen(UNIX_URI_PREFIX)) == 0) {
        return _parse_unix_uri(uri, &stream_ep->addr, &stream_ep->addr_len);
    } else if (strncmp(uri, TCP_URI_PREFIX, strlen(TCP_URI_PREFIX)) == 0) {
        return _parse_tcp_uri(uri, &stream_ep->addr, &stream_ep->addr_len);
    } else {
        log_error("Unsupported stream URI: %s", uri);
        return -EPROTONOSUPPORT;
    }
}


Endpoint* stream_connect(
    const char* uri, uint32_t uid, bool bus_mode, double timeout)
{
    int32_t startup_rc = stream_socket_startup();
    if (startup_rc < 0) {
        log_fatal("Failed to initialize stream socket layer: %s",
            strerror(-startup_rc));
    }

    if (uid == 0) {
        log_error("UID not configured, uid=%u", uid);
    }

    /* Endpoint. */
    Endpoint* endpoint = _create_driver(uri);
    endpoint->uid = uid;
    endpoint->bus_mode = bus_mode;
    int rc = hashmap_init_alt(&endpoint->endpoint_channels, 16, NULL);
    if (rc) {
        log_fatal("Hashmap init failed for endpoint->endpoint_channels!");
    }

    /* Stream Endpoint. */
    StreamEndpoint* stream_ep = calloc(1, sizeof(StreamEndpoint));
    if (stream_ep == NULL) {
        log_fatal("StreamEndpoint malloc failed");
    }
    stream_ep->uri = uri;
    if (_parse_stream_uri(uri, stream_ep) < 0) {
        log_fatal("Failed to parse stream URI: %s", uri);
    }
    log_debug("  Stream:");
    log_debug("    uri: %s", uri);
    if (stream_ep->addr.ss_family == AF_UNIX) {
        const struct sockaddr_un* addr =
            (const struct sockaddr_un*)&stream_ep->addr;
        log_debug("    family: unix");
        log_debug("    path: %s", addr->sun_path);
    } else if (stream_ep->addr.ss_family == AF_INET ||
               stream_ep->addr.ss_family == AF_INET6) {
        char host[STREAM_MAX_HOST_LENGTH] = { 0 };
        char port[16] = { 0 };
        getnameinfo((const struct sockaddr*)&stream_ep->addr,
            stream_ep->addr_len, host, sizeof(host), port, sizeof(port),
            NI_NUMERICHOST | NI_NUMERICSERV);
        log_debug("    family: tcp");
        log_debug("    hostname: %s", host);
        log_debug("    port: %s", port);
    }

    stream_ep->server.simbus.fd = STREAM_INVALID_SOCKET;
    stream_ep->server.models = vector_make(sizeof(StreamInstance), 16, NULL);
    stream_ep->server.poll_fds = vector_make(sizeof(stream_pollfd_t), 16, NULL);
    stream_ep->server.rx_messages =
        vector_make(sizeof(StreamMessage), 16, NULL);
    stream_ep->client.model.fd = STREAM_INVALID_SOCKET;

    if (endpoint->bus_mode) {
        stream_ep->server.simbus.uid = uid;
        log_debug("  Endpoint: ");
        log_debug("    Model UID: %i", stream_ep->server.simbus.uid);
    } else {
        stream_ep->client.model.uid = uid;
        log_debug("  Endpoint: ");
        log_debug("    Model UID: %i", stream_ep->client.model.uid);
    }
    stream_ep->recv_timeout = timeout;

    /* Return the endpoint*/
    endpoint->private = (void*)stream_ep;
    return endpoint;
}


void stream_instance_free_recv_buffer(StreamInstance* si)
{
    if (si == NULL) return;

    free(si->recv_buffer.data);
    si->recv_buffer.data = NULL;
    si->recv_buffer.capacity = 0;
    si->recv_buffer.length = 0;
}


static void _free_rx_message(void* item, void* data)
{
    UNUSED(data);

    StreamMessage* msg = item;
    if (msg) {
        free(msg->data);
        msg->data = NULL;
        msg->length = 0;
        msg->fd = STREAM_INVALID_SOCKET;
    }
}


int32_t stream_queue_rx_message(StreamEndpoint* ep, stream_socket_t fd,
    const uint8_t* data, uint32_t length)
{
    uint8_t* copy = malloc(length);
    if (copy == NULL) {
        return -ENOMEM;
    }

    memcpy(copy, data, length);

    StreamMessage msg = {
        .fd = fd,
        .data = copy,
        .length = length,
    };

    vector_push(&ep->server.rx_messages, &msg);
    ep->server.diagnostics.rx_messages_queued++;

    return 0;
}


int32_t stream_pop_rx_message(
    StreamEndpoint* ep, uint8_t** buffer, uint32_t* buffer_length)
{
    if (vector_len(&ep->server.rx_messages) == 0) {
        return 0;
    }

    StreamMessage* msg = vector_at(&ep->server.rx_messages, 0, NULL);
    if (msg == NULL || msg->data == NULL || msg->length == 0) {
        vector_delete_at(&ep->server.rx_messages, 0);
        return 0;
    }

    uint32_t        msg_length = msg->length;
    stream_socket_t msg_fd = msg->fd;

    if (msg_length > *buffer_length) {
        uint8_t* b = realloc(*buffer, msg_length);
        if (b == NULL) {
            return -ENOMEM;
        }

        *buffer = b;
        *buffer_length = msg_length;
    }

    memcpy(*buffer, msg->data, msg_length);

    free(msg->data);
    msg->data = NULL;
    msg->length = 0;
    msg->fd = STREAM_INVALID_SOCKET;

    vector_delete_at(&ep->server.rx_messages, 0);

    ep->server.diagnostics.rx_cached_returns++;

    log_debug("stream recv_fbs cached return: "
              "fd=" STREAM_SOCKET_LOG_FORMAT
              " length=%u cached_returns=%" PRIu64 " queued_total=%" PRIu64
              " poll_count=%" PRIu64 " recv_calls=%" PRIu64,
        STREAM_SOCKET_LOG_VALUE(msg_fd), msg_length,
        ep->server.diagnostics.rx_cached_returns,
        ep->server.diagnostics.rx_messages_queued,
        ep->server.diagnostics.rx_poll_count,
        ep->server.diagnostics.rx_recv_calls);

    return (int32_t)msg_length;
}

int32_t stream_instance_reserve_recv_buffer(StreamInstance* si, uint32_t needed)
{
    if (needed <= si->recv_buffer.capacity) {
        return 0;
    }

    uint32_t new_capacity =
        si->recv_buffer.capacity ? si->recv_buffer.capacity : 1024;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    uint8_t* b = realloc(si->recv_buffer.data, new_capacity);
    if (b == NULL) {
        return -ENOMEM;
    }

    si->recv_buffer.data = b;
    si->recv_buffer.capacity = new_capacity;

    return 0;
}


void stream_endpoint_destroy(Endpoint* endpoint)
{
    if (endpoint && endpoint->private) {
        StreamEndpoint* stream_ep = endpoint->private;

        // Statistics.
        uint64_t tx_total = 0;
        uint64_t rx_total = 0;
        for (size_t bucket = 0;
             bucket < STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS; bucket++) {
            tx_total += stream_ep->message_size_histogram.tx[bucket];
            rx_total += stream_ep->message_size_histogram.rx[bucket];
        }

        size_t final_bucket = STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS - 1;
        if (stream_ep->message_size_histogram.tx[final_bucket] > 0 ||
            stream_ep->message_size_histogram.rx[final_bucket] > 0) {
            log_error("Stream message sizes (OVERFLOW DETECTED):");
        } else {
            log_info("Stream message sizes:");
        }
        log_info("    %11s %8s %8s", "bucket", "tx(%)", "rx(%)");
        for (size_t bucket = 0; bucket < STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS;
            bucket++) {
            uint64_t tx_count = stream_ep->message_size_histogram.tx[bucket];
            uint64_t rx_count = stream_ep->message_size_histogram.rx[bucket];
            if (tx_count == 0 && rx_count == 0) continue;

            if (bucket + 1 == STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS) {
                log_error("    %11s %8.3f %8.3f", ">64 MiB",
                    tx_total ? 100.0 * tx_count / tx_total : 0.0,
                    rx_total ? 100.0 * rx_count / rx_total : 0.0);
            } else {
                uint64_t upper_bound = 1ULL << (10U + bucket);
                log_info("      %5" PRIu64 " KiB %8.3f %8.3f",
                    upper_bound / 1024U,
                    tx_total ? 100.0 * tx_count / tx_total : 0.0,
                    rx_total ? 100.0 * rx_count / rx_total : 0.0);
            }
        }

        // vector_clear();
        vector_reset(&stream_ep->server.models);
        vector_reset(&stream_ep->server.poll_fds);

        vector_clear(&stream_ep->server.rx_messages, _free_rx_message, NULL);
        vector_reset(&stream_ep->server.rx_messages);

        free(endpoint->private);
        endpoint->private = NULL;
    }
    if (endpoint) {
        hashmap_destroy(&endpoint->endpoint_channels);
        free(endpoint);
    }
}


void* stream_create_channel(Endpoint* endpoint, const char* name)
{
    assert(endpoint);
    assert(endpoint->private);
    hashmap_set(&endpoint->endpoint_channels, name, (void*)name);
    log_debug("    Endpoint Channel : %s", name);
    return (void*)name;
}


uint64_t stream_time_ns(void)
{
    struct timespec ts = { 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}


void stream_interrupt(Endpoint* endpoint)
{
    assert(endpoint);
    endpoint->stop_request = 1;
}
