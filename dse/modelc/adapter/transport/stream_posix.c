// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/adapter/transport/stream.h>


#define UNUSED(x) ((void)x)


static int32_t _configure_socket(stream_socket_t fd, sa_family_t family)
{
    if (family == AF_INET || family == AF_INET6) {
        int enabled = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&enabled,
                sizeof(enabled)) < 0) {
            return -stream_socket_errno();
        }
    }

    return stream_configure_socket_timeout(fd);
}


static void _stream_instance_disconnect(StreamInstance* si)
{
    if (si && STREAM_SOCKET_IS_VALID(si->fd)) {
        STREAM_SOCKET_CLOSE(si->fd);
        si->fd = STREAM_INVALID_SOCKET;
    }

    if (si) {
        stream_instance_free_recv_buffer(si);
    }
}


static void _disconnect(StreamEndpoint* ep, stream_socket_t fd)
{
    if (ep == NULL || !STREAM_SOCKET_IS_VALID(fd)) return;

    int saved_errno = errno;
    STREAM_SOCKET_CLOSE(fd);

    size_t model_count = vector_len(&ep->server.models);
    for (size_t i = 0; i < model_count; i++) {
        StreamInstance* si = vector_at(&ep->server.models, i, NULL);
        if (si && si->fd == fd) {
            si->fd = STREAM_INVALID_SOCKET;
        }
    }

    size_t poll_count = vector_len(&ep->server.poll_fds);
    for (size_t i = 0; i < poll_count; i++) {
        stream_pollfd_t* poll_fd = vector_at(&ep->server.poll_fds, i, NULL);
        if (poll_fd && poll_fd->fd == fd) {
            poll_fd->fd = STREAM_INVALID_SOCKET;
            poll_fd->events = 0;
            poll_fd->revents = 0;
        }
    }

    errno = saved_errno;
}


static int32_t _stream_instance_read_recv_buffer(
    StreamEndpoint* ep, StreamInstance* si)
{
    if (si->recv_buffer.length == si->recv_buffer.capacity) {
        int32_t rc = stream_instance_reserve_recv_buffer(
            si, si->recv_buffer.length + 65536U);
        if (rc < 0) {
            return rc;
        }
    }

    ssize_t r =
        recv(si->fd, (char*)si->recv_buffer.data + si->recv_buffer.length,
            si->recv_buffer.capacity - si->recv_buffer.length, MSG_DONTWAIT);

    if (__log_level__ <= LOG_DEBUG) {
        ep->server.diagnostics.rx_recv_calls++;
    }

    if (r > 0) {
        si->recv_buffer.length += (uint32_t)r;

        log_debug("stream rx read nonblocking once: "
                  "fd=" STREAM_SOCKET_LOG_FORMAT
                  " read=%zd recv_length=%u recv_calls=%" PRIu64,
            STREAM_SOCKET_LOG_VALUE(si->fd), r, si->recv_buffer.length,
            ep->server.diagnostics.rx_recv_calls);

        return (int32_t)r;
    } else if (r == 0) {
        log_debug("stream rx read disconnect: "
                  "fd=" STREAM_SOCKET_LOG_FORMAT " recv_calls=%" PRIu64,
            STREAM_SOCKET_LOG_VALUE(si->fd),
            ep->server.diagnostics.rx_recv_calls);

        return -ECONNRESET;
    } else {
        int error = stream_socket_errno();

        if (error == EINTR || error == EAGAIN || error == EWOULDBLOCK) {
            return 0;
        }

        log_debug("stream rx read failed: "
                  "fd=" STREAM_SOCKET_LOG_FORMAT
                  " errno=%d error=%s recv_calls=%" PRIu64,
            STREAM_SOCKET_LOG_VALUE(si->fd), error, strerror(error),
            ep->server.diagnostics.rx_recv_calls);

        return -error;
    }
}


static int32_t _stream_instance_extract_recv_messages(
    StreamEndpoint* ep, StreamInstance* si)
{
    while (si->recv_buffer.length >= sizeof(uint32_t)) {
        uint32_t size_prefix = 0;
        memcpy(&size_prefix, si->recv_buffer.data, sizeof(size_prefix));

        uint32_t payload_length = stream_le32toh(size_prefix);
        if (payload_length >
            (64U * 1024U * 1024U) - (uint32_t)sizeof(size_prefix)) {
            return -EMSGSIZE;
        }

        uint32_t message_len = payload_length + (uint32_t)sizeof(size_prefix);
        if (si->recv_buffer.length < message_len) {
            break;
        }

        int32_t rc = stream_queue_rx_message(
            ep, si->fd, si->recv_buffer.data, message_len);
        if (rc < 0) {
            return rc;
        }

        uint32_t remaining = si->recv_buffer.length - message_len;
        if (remaining > 0) {
            memmove(si->recv_buffer.data, si->recv_buffer.data + message_len,
                remaining);
        }

        si->recv_buffer.length = remaining;

        log_debug("stream rx queued complete message: "
                  "fd=" STREAM_SOCKET_LOG_FORMAT
                  " message_len=%u remaining_recv=%u queue_len=%zu",
            STREAM_SOCKET_LOG_VALUE(si->fd), message_len,
            si->recv_buffer.length, vector_len(&ep->server.rx_messages));
    }

    return 0;
}


static int32_t _drain_readable_client(StreamEndpoint* ep, StreamInstance* si)
{
    if (si == NULL) {
        return -ENOENT;
    }

    for (;;) {
        int32_t read_length = _stream_instance_read_recv_buffer(ep, si);
        if (read_length < 0) {
            return read_length;
        }

        int32_t rc = _stream_instance_extract_recv_messages(ep, si);
        if (rc < 0) {
            return rc;
        }

        if (read_length == 0) {
            return 0;
        }
    }
}


static void _remove_stale_clients(StreamEndpoint* ep)
{
    size_t model_count = vector_len(&ep->server.models);
    size_t poll_count = vector_len(&ep->server.poll_fds);
    if (poll_count == 0 || model_count + 1 != poll_count) {
        log_error("Stream client vectors out of sync: models=%zu, poll_fds=%zu",
            model_count, poll_count);
        return;
    }

    for (size_t i = model_count; i > 0; i--) {
        size_t           index = i - 1;
        StreamInstance*  si = vector_at(&ep->server.models, index, NULL);
        stream_pollfd_t* poll_fd =
            vector_at(&ep->server.poll_fds, index + 1, NULL);

        assert(si);
        assert(poll_fd);

        if (!STREAM_SOCKET_IS_VALID(si->fd) ||
            !STREAM_SOCKET_IS_VALID(poll_fd->fd)) {
            stream_instance_free_recv_buffer(si);
            vector_delete_at(&ep->server.models, index);
            vector_delete_at(&ep->server.poll_fds, index + 1);
        }
    }
}


static int32_t _drain_readable_clients(StreamEndpoint* ep)
{
    size_t fds_count = vector_len(&ep->server.poll_fds);

    for (size_t i = 1; i < fds_count; i++) {
        stream_pollfd_t* poll_fd = vector_at(&ep->server.poll_fds, i, NULL);
        if (poll_fd == NULL || !STREAM_SOCKET_IS_VALID(poll_fd->fd)) {
            continue;
        }

        if (poll_fd->revents == 0) {
            continue;
        }

        stream_socket_t fd = poll_fd->fd;
        StreamInstance* si = vector_at(&ep->server.models, i - 1, NULL);

        if (poll_fd->revents & POLLNVAL) {
            log_info("Stream client poll invalid, fd=" STREAM_SOCKET_LOG_FORMAT,
                STREAM_SOCKET_LOG_VALUE(fd));
            _disconnect(ep, fd);
            continue;
        }

        if (poll_fd->revents & (POLLIN | POLLHUP | POLLERR)) {
            int32_t rc = _drain_readable_client(ep, si);

            if (rc == -ECONNRESET || rc == -EPIPE) {
                log_info(
                    "Stream client disconnected, fd=" STREAM_SOCKET_LOG_FORMAT,
                    STREAM_SOCKET_LOG_VALUE(fd));
                _disconnect(ep, fd);
                continue;
            }

            if (rc < 0) {
                log_error("Failed to drain readable stream client, "
                          "fd=" STREAM_SOCKET_LOG_FORMAT ": %s",
                    STREAM_SOCKET_LOG_VALUE(fd), strerror(-rc));
                _disconnect(ep, fd);
                continue;
            }

            if (poll_fd->revents & (POLLHUP | POLLERR)) {
                log_info("Stream client closed after drain, "
                         "fd=" STREAM_SOCKET_LOG_FORMAT " revents=0x%x",
                    STREAM_SOCKET_LOG_VALUE(fd), poll_fd->revents);
                _disconnect(ep, fd);
                continue;
            }
        }
    }

    _remove_stale_clients(ep);
    return 0;
}


static void _accept_new_clients(StreamEndpoint* ep)
{
    for (;;) {
        stream_socket_t client_fd = accept(ep->server.simbus.fd, NULL, NULL);
        if (!STREAM_SOCKET_IS_VALID(client_fd)) {
            int error = stream_socket_errno();
            if (error == EAGAIN || error == EWOULDBLOCK) {
                return;
            }
            log_error("Failed to accept client: %s", strerror(error));
            return;
        }

        int32_t rc = _configure_socket(client_fd, ep->addr.ss_family);
        if (rc < 0) {
            log_error("Failed to configure client socket, "
                      "fd=" STREAM_SOCKET_LOG_FORMAT ": %s",
                STREAM_SOCKET_LOG_VALUE(client_fd), strerror(-rc));
            STREAM_SOCKET_CLOSE(client_fd);
            continue;
        }

        rc = stream_socket_set_nonblocking(client_fd);
        if (rc < 0) {
            log_error("Failed to set client socket nonblocking, "
                      "fd=" STREAM_SOCKET_LOG_FORMAT ": %s",
                STREAM_SOCKET_LOG_VALUE(client_fd), strerror(-rc));
            STREAM_SOCKET_CLOSE(client_fd);
            continue;
        }

        vector_push(&ep->server.models,
            &(StreamInstance){
                .fd = client_fd,
                .uid = 0,
                .recv_buffer = { .data = NULL, .length = 0, .capacity = 0 },
            });
        vector_push(&ep->server.poll_fds,
            &(stream_pollfd_t){ .fd = client_fd, .events = POLLIN });
        log_info("Client connected, fd=" STREAM_SOCKET_LOG_FORMAT,
            STREAM_SOCKET_LOG_VALUE(client_fd));
    }
}


static ssize_t _recv_exact(stream_socket_t fd, uint8_t* buffer, uint32_t length)
{
    uint32_t received = 0;

    while (received < length) {
        ssize_t rc =
            recv(fd, (char*)buffer + received, (int)(length - received), 0);
        if (rc > 0) {
            received += (uint32_t)rc;
        } else if (rc == 0) {
            if (received > 0) {
                errno = EPIPE;
                return -1;
            }
            return 0;
        } else {
            int error = stream_socket_errno();
            if (received > 0) {
                errno = EPIPE;
            } else if (error == EINTR || error == EAGAIN ||
                       error == EWOULDBLOCK) {
                errno = EAGAIN;
            } else {
                errno = error;
            }
            return -1;
        }
    }

    return (ssize_t)received;
}


static ssize_t _recv_message(
    StreamInstance* si, uint8_t** buffer, uint32_t* buffer_length)
{
    uint32_t size_prefix = 0;

    ssize_t p = _recv_exact(
        si->fd, (uint8_t*)&size_prefix, (uint32_t)sizeof(size_prefix));
    if (p == 0) {
        return 0;
    } else if (p < 0) {
        return -1;
    }

    uint32_t payload_length = stream_le32toh(size_prefix);
    if (payload_length >
        (64U * 1024U * 1024U) - (uint32_t)sizeof(size_prefix)) {
        errno = EMSGSIZE;
        return -1;
    }

    uint32_t message_len = payload_length + (uint32_t)sizeof(size_prefix);
    if (message_len > *buffer_length) {
        uint8_t* b = realloc(*buffer, message_len);
        if (b == NULL) {
            errno = ENOMEM;
            return -1;
        }

        *buffer = b;
        *buffer_length = message_len;
    }

    memcpy(*buffer, &size_prefix, sizeof(size_prefix));

    ssize_t r =
        _recv_exact(si->fd, *buffer + sizeof(size_prefix), payload_length);
    if (r == (ssize_t)payload_length) {
        return (ssize_t)message_len;
    } else if (r == 0) {
        return 0;
    } else if (r < 0) {
        return -1;
    } else {
        errno = EPIPE;
        return -1;
    }
}


static int32_t _send_msg(
    stream_socket_t fd, const void* buffer, uint32_t length)
{
    const uint8_t* data = buffer;
    size_t         remaining = length;

    while (remaining > 0) {
        ssize_t sent =
            send(fd, (const char*)data, (int)remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            int error = stream_socket_errno();
            if (error == EINTR) {
                continue;
            }
            return -error;
        } else if (sent == 0) {
            return -EPIPE;
        } else {
            data += sent;
            remaining -= (size_t)sent;
        }
    }

    return 0;
}


int32_t stream_posix_start(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    StreamEndpoint* stream_ep = endpoint->private;

    if (endpoint->bus_mode) {
        stream_ep->server.simbus.fd =
            socket(stream_ep->addr.ss_family, SOCK_STREAM, 0);
        if (!STREAM_SOCKET_IS_VALID(stream_ep->server.simbus.fd)) {
            return -stream_socket_errno();
        }

        if (stream_ep->addr.ss_family == AF_UNIX) {
            struct sockaddr_un* unix_addr =
                (struct sockaddr_un*)&stream_ep->addr;
            STREAM_UNLINK(unix_addr->sun_path);
        } else if (stream_ep->addr.ss_family == AF_INET ||
                   stream_ep->addr.ss_family == AF_INET6) {
            int32_t rc =
                stream_configure_listener_socket(stream_ep->server.simbus.fd);
            if (rc < 0) {
                _stream_instance_disconnect(&stream_ep->server.simbus);
                return rc;
            }
        }

        if (bind(stream_ep->server.simbus.fd,
                (struct sockaddr*)&stream_ep->addr, stream_ep->addr_len) < 0) {
            int error = stream_socket_errno();
            _stream_instance_disconnect(&stream_ep->server.simbus);
            return -error;
        }

        if (listen(stream_ep->server.simbus.fd, SOMAXCONN) < 0) {
            int error = stream_socket_errno();
            log_error("Failed to listen on stream socket: %s", strerror(error));
            _stream_instance_disconnect(&stream_ep->server.simbus);
            return -error;
        }

        int32_t rc = stream_socket_set_nonblocking(stream_ep->server.simbus.fd);
        if (rc < 0) {
            log_error(
                "Failed to set simbus socket nonblocking: %s", strerror(-rc));
            _stream_instance_disconnect(&stream_ep->server.simbus);
            return rc;
        }

        vector_push(
            &stream_ep->server.poll_fds, &(stream_pollfd_t){
                                             .fd = stream_ep->server.simbus.fd,
                                             .events = POLLIN,
                                         });

        log_info("Server (bus) listening");
        return 0;
    }

    int32_t connect_rc = stream_socket_connect_retry(
        &stream_ep->client.model.fd, stream_ep->addr.ss_family,
        (struct sockaddr*)&stream_ep->addr, stream_ep->addr_len);
    if (connect_rc < 0) {
        log_error("Client (model) connect timeout");
        return connect_rc;
    }

    int32_t rc = _configure_socket(
        stream_ep->client.model.fd, stream_ep->addr.ss_family);
    if (rc < 0) {
        log_error("Failed to configure client socket, "
                  "fd=" STREAM_SOCKET_LOG_FORMAT ": %s",
            STREAM_SOCKET_LOG_VALUE(stream_ep->client.model.fd), strerror(-rc));
        _stream_instance_disconnect(&stream_ep->client.model);
        return rc;
    }

    log_info("Client (model) connected");
    return 0;
}


int32_t stream_posix_send_fbs(Endpoint* endpoint, void* endpoint_channel,
    void* buffer, uint32_t buffer_length, uint32_t model_uid)
{
    UNUSED(model_uid);
    UNUSED(endpoint_channel);
    assert(endpoint);
    assert(endpoint->private);
    StreamEndpoint* stream_ep = endpoint->private;
    int32_t         rc = 0;

    if (endpoint->bus_mode) {
        size_t count = vector_len(&stream_ep->server.models);
        for (size_t i = 0; i < count; i++) {
            StreamInstance* si = vector_at(&stream_ep->server.models, i, NULL);
            if (si) {
                int32_t send_rc = _send_msg(si->fd, buffer, buffer_length);
                if (send_rc < 0) {
                    log_error(
                        "Failed to send to client, fd=" STREAM_SOCKET_LOG_FORMAT
                        ": %s",
                        STREAM_SOCKET_LOG_VALUE(si->fd), strerror(-send_rc));
                    _disconnect(stream_ep, si->fd);
                    rc = -1;
                }
            }
        }
    } else {
        int32_t send_rc =
            _send_msg(stream_ep->client.model.fd, buffer, buffer_length);
        if (send_rc < 0) {
            log_error("Failed to send to server, fd=" STREAM_SOCKET_LOG_FORMAT
                      ": %s",
                STREAM_SOCKET_LOG_VALUE(stream_ep->client.model.fd),
                strerror(-send_rc));
            _stream_instance_disconnect(&stream_ep->client.model);
            rc = -1;
        }
    }

    return rc;
}


int32_t stream_posix_recv_fbs(Endpoint* endpoint, const char** channel_name,
    uint8_t** buffer, uint32_t* buffer_length)
{
    assert(endpoint);
    assert(endpoint->private);
    assert(channel_name);

    StreamEndpoint* stream_ep = endpoint->private;
    uint32_t        timeout_counter = (uint32_t)stream_ep->recv_timeout + 1;

    *channel_name = NULL;

    uint64_t function_start_ns = 0;
    uint32_t loop_count = 0;

    if (__log_level__ <= LOG_DEBUG) {
        function_start_ns = stream_time_ns();
    }

    while (--timeout_counter) {
        if (__log_level__ <= LOG_DEBUG) {
            loop_count++;
        }

        if (endpoint->stop_request) {
            log_debug("stream recv_fbs stop requested: "
                      "bus_mode=%d loops=%u total_duration_ns=%" PRIu64,
                endpoint->bus_mode, loop_count,
                stream_time_ns() - function_start_ns);
            return 0;
        }

        if (endpoint->bus_mode) {
            uint64_t loop_start_ns = 0;
            uint64_t poll_start_ns = 0;
            uint64_t poll_duration_ns = 0;
            size_t   fds_count;

            if (__log_level__ <= LOG_DEBUG) {
                loop_start_ns = stream_time_ns();
            }

            int32_t cached_len =
                stream_pop_rx_message(stream_ep, buffer, buffer_length);
            if (cached_len < 0) {
                errno = -cached_len;
                return -1;
            } else if (cached_len > 0) {
                log_debug("stream recv_fbs bus cache hit: "
                          "length=%d loop_total_ns=%" PRIu64,
                    cached_len, stream_time_ns() - loop_start_ns);
                return cached_len;
            }

            fds_count = vector_len(&stream_ep->server.poll_fds);

            if (__log_level__ <= LOG_DEBUG) {
                poll_start_ns = stream_time_ns();
            }
            int activity =
                STREAM_POLL(stream_ep->server.poll_fds.items, fds_count, 1000);

            if (__log_level__ <= LOG_DEBUG) {
                poll_duration_ns = stream_time_ns() - poll_start_ns;
                stream_ep->server.diagnostics.rx_poll_count++;
            }

            if (activity == 0) {
                log_debug("stream recv_fbs bus poll timeout: "
                          "fds=%zu poll_count=%" PRIu64 " duration_ns=%" PRIu64,
                    fds_count, stream_ep->server.diagnostics.rx_poll_count,
                    poll_duration_ns);
                errno = EAGAIN;
                continue;
            } else if (activity < 0) {
                int error = stream_socket_errno();
                log_debug("stream recv_fbs bus poll failed: "
                          "fds=%zu errno=%d error=%s duration_ns=%" PRIu64,
                    fds_count, error, strerror(error), poll_duration_ns);

                if (error == EINTR) {
                    errno = EAGAIN;
                    continue;
                }

                errno = error;
                return -1;
            } else {
                log_debug("stream recv_fbs bus poll ready: "
                          "fds=%zu activity=%d "
                          "poll_count=%" PRIu64 " duration_ns=%" PRIu64,
                    fds_count, activity,
                    stream_ep->server.diagnostics.rx_poll_count,
                    poll_duration_ns);

                stream_pollfd_t* listener_poll_fd =
                    vector_at(&stream_ep->server.poll_fds, 0, NULL);
                int listener_error =
                    stream_listener_poll_error(listener_poll_fd);
                if (listener_error < 0) {
                    log_error("Stream listener poll failed: revents=0x%x: %s",
                        listener_poll_fd->revents, strerror(-listener_error));
                    errno = -listener_error;
                    return -1;
                } else if (listener_poll_fd &&
                           listener_poll_fd->revents & POLLIN) {
                    _accept_new_clients(stream_ep);
                }

                int32_t drain_rc = _drain_readable_clients(stream_ep);
                if (drain_rc < 0) {
                    errno = -drain_rc;
                    return -1;
                }

                cached_len =
                    stream_pop_rx_message(stream_ep, buffer, buffer_length);
                if (cached_len < 0) {
                    errno = -cached_len;
                    return -1;
                } else if (cached_len > 0) {
                    log_debug("stream recv_fbs bus drained return: "
                              "length=%d queue_remaining=%zu "
                              "loop_total_ns=%" PRIu64,
                        cached_len, vector_len(&stream_ep->server.rx_messages),
                        stream_time_ns() - loop_start_ns);
                    return cached_len;
                }

                log_debug(
                    "stream recv_fbs bus no complete message after drain: "
                    "activity=%d loop_total_ns=%" PRIu64,
                    activity, stream_time_ns() - loop_start_ns);
                continue;
            }
        }

        uint64_t recv_start_ns = 0;
        uint64_t recv_duration_ns = 0;

        if (__log_level__ <= LOG_DEBUG) {
            recv_start_ns = stream_time_ns();
        }
        ssize_t size =
            _recv_message(&stream_ep->client.model, buffer, buffer_length);

        if (__log_level__ <= LOG_DEBUG) {
            recv_duration_ns = stream_time_ns() - recv_start_ns;
        }

        log_debug("stream recv_fbs model message: "
                  "fd=" STREAM_SOCKET_LOG_FORMAT
                  " size=%zd loops=%u recv_total_ns=%" PRIu64
                  " function_total_ns=%" PRIu64,
            STREAM_SOCKET_LOG_VALUE(stream_ep->client.model.fd), size,
            loop_count, recv_duration_ns, stream_time_ns() - function_start_ns);

        if (size > 0) {
            return (int32_t)size;
        } else if (size == 0) {
            _stream_instance_disconnect(&stream_ep->client.model);
            errno = ECONNRESET;
            return -1;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            continue;
        } else {
            _stream_instance_disconnect(&stream_ep->client.model);
            return -1;
        }
    }

    log_debug("stream_recv_fbs timeout: "
              "bus_mode=%d loops=%u total_duration_ns=%" PRIu64,
        endpoint->bus_mode, loop_count, stream_time_ns() - function_start_ns);
    log_trace("stream_recv_fbs: no message (timeout)");
    errno = ETIME;
    return -1;
}


static void _stream_instance_close_item(void* item, void* data)
{
    UNUSED(data);
    _stream_instance_disconnect((StreamInstance*)item);
}


void stream_posix_disconnect(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    StreamEndpoint* stream_ep = endpoint->private;

    _stream_instance_close_item(&stream_ep->server.simbus, NULL);
    vector_clear(&stream_ep->server.models, _stream_instance_close_item, NULL);
    vector_clear(&stream_ep->server.poll_fds, NULL, NULL);
    _stream_instance_close_item(&stream_ep->client.model, NULL);

    stream_endpoint_destroy(endpoint);
}
