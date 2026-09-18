// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_STREAM_LINUX_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_STREAM_LINUX_H_

#ifndef _WIN32

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <endian.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <asm/ioctls.h>
#endif

typedef int           stream_socket_t;
typedef struct pollfd stream_pollfd_t;


#define STREAM_INVALID_SOCKET              (-1)
#define STREAM_SOCKET_IS_VALID(fd)         ((fd) >= 0)
#define STREAM_SOCKET_CLOSE(fd)            close(fd)

#define STREAM_POLL(fds, nfds, timeout_ms) poll((fds), (nfds), (timeout_ms))

#define STREAM_SLEEP_MS(ms)                usleep((ms) * 1000U)
#define STREAM_UNLINK(path)                unlink(path)

#define STREAM_SOCKET_LOG_FORMAT           "%d"
#define STREAM_SOCKET_LOG_VALUE(fd)        ((int)(fd))

#define stream_le32toh(value)              le32toh(value)


static inline int stream_socket_startup(void)
{
    return 0;
}


static inline int stream_socket_errno(void)
{
    return errno;
}

static inline int stream_socket_bytes_available(
    stream_socket_t fd, uint32_t* available)
{
    int count = 0;

    if (ioctl(fd, FIONREAD, &count) < 0) {
        return -errno;
    }

    if (count < 0) {
        count = 0;
    }

    *available = (uint32_t)count;
    return 0;
}

static inline int stream_socket_set_nonblocking(stream_socket_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -errno;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -errno;
    }

    return 0;
}

static inline int stream_configure_socket_buffers(stream_socket_t fd)
{
    int buffer_length = (int)STREAM_SOCKET_BUFFER_LENGTH;

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer_length,
            sizeof(buffer_length)) < 0) {
        return -errno;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer_length,
            sizeof(buffer_length)) < 0) {
        return -errno;
    }

    return 0;
}

static inline int stream_configure_listener_socket(stream_socket_t fd)
{
    int enabled = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) <
        0) {
        return -errno;
    }

    return stream_configure_socket_buffers(fd);
}

static inline int32_t stream_socket_connect_retry(stream_socket_t* fd,
    sa_family_t family, const struct sockaddr* addr, socklen_t addr_len)
{
    for (uint32_t i = 0; i < 5000U; i++) {
        *fd = socket(family, SOCK_STREAM, 0);
        if (!STREAM_SOCKET_IS_VALID(*fd)) {
            return -errno;
        }

        if (connect(*fd, addr, addr_len) >= 0) {
            return 0;
        }

        STREAM_SOCKET_CLOSE(*fd);
        *fd = STREAM_INVALID_SOCKET;
        STREAM_SLEEP_MS(1);
    }

    return -EAGAIN;
}

static inline int stream_listener_poll_error(const stream_pollfd_t* poll_fd)
{
    (void)poll_fd;
    return 0;
}

static inline int stream_configure_socket_timeout(stream_socket_t fd)
{
    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0,
    };

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return -errno;
    }

    struct timeval tvS = {
        .tv_sec = 0,
        .tv_usec = 1000,
    };

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tvS, sizeof(tv)) < 0) {
        return -errno;
    }

    return 0;
}


static inline int32_t stream_getaddrinfo_error(int rc)
{
    switch (rc) {
    case EAI_AGAIN:
        return -EAGAIN;

    case EAI_MEMORY:
        return -ENOMEM;

    case EAI_SYSTEM:
        return errno ? -errno : -EIO;

    default:
        return -EINVAL;
    }
}

#else
#error "stream_linux.h must not be included on Windows"
#endif

#endif  // DSE_MODELC_ADAPTER_TRANSPORT_STREAM_LINUX_H_
