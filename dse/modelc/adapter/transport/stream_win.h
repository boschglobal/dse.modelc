// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_STREAM_WIN_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_STREAM_WIN_H_

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <basetsd.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <io.h>
#include <windows.h>


typedef SOCKET    stream_socket_t;
typedef WSAPOLLFD stream_pollfd_t;

#ifndef sa_family_t
typedef ADDRESS_FAMILY sa_family_t;
#endif

#ifndef socklen_t
typedef int socklen_t;
#endif


#ifndef EAGAIN
#define EAGAIN EWOULDBLOCK
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifndef EPIPE
#define EPIPE 32
#endif

#ifndef EMSGSIZE
#define EMSGSIZE 90
#endif

#ifndef ENOTCONN
#define ENOTCONN 107
#endif

#ifndef ECONNRESET
#define ECONNRESET 104
#endif

#ifndef ECONNABORTED
#define ECONNABORTED 103
#endif

#ifndef ECONNREFUSED
#define ECONNREFUSED 111
#endif

#ifndef ENOTSOCK
#define ENOTSOCK 88
#endif

#ifndef ENOBUFS
#define ENOBUFS 105
#endif

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 95
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

#ifndef EADDRINUSE
#define EADDRINUSE 98
#endif

#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL 99
#endif

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 97
#endif

#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT 93
#endif


#define STREAM_INVALID_SOCKET      INVALID_SOCKET
#define STREAM_SOCKET_IS_VALID(fd) ((fd) != INVALID_SOCKET)
#define STREAM_SOCKET_CLOSE(fd)    closesocket(fd)

#define STREAM_POLL(fds, nfds, timeout_ms)                                     \
    WSAPoll((fds), (ULONG)(nfds), (timeout_ms))

#define STREAM_SLEEP_MS(ms)         Sleep(ms)
#define STREAM_UNLINK(path)         _unlink(path)

#define STREAM_SOCKET_LOG_FORMAT    "%llu"
#define STREAM_SOCKET_LOG_VALUE(fd) ((uint64_t)(fd))

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif


static inline uint32_t stream_le32toh(uint32_t value)
{
    return value;
}


static inline void stream_socket_cleanup(void)
{
    WSACleanup();
}


static inline int stream_socket_startup(void)
{
    static int initialized = 0;

    if (!initialized) {
        WSADATA wsa_data;
        int     rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (rc != 0) {
            return -rc;
        }

        if (atexit(stream_socket_cleanup) != 0) {
            WSACleanup();
            return -ENOMEM;
        }

        initialized = 1;
    }

    return 0;
}


static inline int stream_socket_error_from_native(int error)
{
    switch (error) {
    case WSAEINTR:
        return EINTR;

    case WSAEWOULDBLOCK:
    case WSAEINPROGRESS:
    case WSAEALREADY:
        return EAGAIN;

    case WSAECONNRESET:
        return ECONNRESET;

    case WSAECONNABORTED:
        return ECONNABORTED;

    case WSAECONNREFUSED:
        return ECONNREFUSED;

    case WSAENOTCONN:
        return ENOTCONN;

    case WSAENOTSOCK:
        return ENOTSOCK;

    case WSAEACCES:
        return EACCES;

    case WSAEINVAL:
        return EINVAL;

    case WSAESHUTDOWN:
        return EPIPE;

    case WSAENOBUFS:
        return ENOBUFS;

    case WSAEOPNOTSUPP:
        return EOPNOTSUPP;

    case WSAEAFNOSUPPORT:
        return EAFNOSUPPORT;

    case WSAEPROTONOSUPPORT:
        return EPROTONOSUPPORT;

    case WSAETIMEDOUT:
        return ETIMEDOUT;

    case WSAEMSGSIZE:
        return EMSGSIZE;

    case WSAEADDRINUSE:
        return EADDRINUSE;

    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;

    case WSAENETDOWN:
    case WSAENETRESET:
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
        return EIO;

    default:
        return EIO;
    }
}


static inline int stream_socket_errno(void)
{
    return stream_socket_error_from_native(WSAGetLastError());
}


static inline int stream_socket_set_nonblocking(stream_socket_t fd)
{
    u_long enabled = 1;
    if (ioctlsocket(fd, FIONBIO, &enabled) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    return 0;
}


static inline int stream_socket_set_blocking(stream_socket_t fd)
{
    u_long enabled = 0;
    if (ioctlsocket(fd, FIONBIO, &enabled) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    return 0;
}


static inline int stream_configure_socket_buffers(stream_socket_t fd)
{
    int buffer_length = (int)STREAM_SOCKET_BUFFER_LENGTH;

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&buffer_length,
            sizeof(buffer_length)) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&buffer_length,
            sizeof(buffer_length)) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    return 0;
}

static inline int stream_configure_listener_socket(stream_socket_t fd)
{
    int enabled = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&enabled,
            sizeof(enabled)) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    return stream_configure_socket_buffers(fd);
}


static inline int stream_socket_connect_once(
    stream_socket_t fd, const struct sockaddr* addr, socklen_t addr_len)
{
    int rc = stream_socket_set_nonblocking(fd);
    if (rc < 0) {
        return rc;
    }

    if (connect(fd, addr, addr_len) < 0) {
        int error = stream_socket_errno();
        if (error != EAGAIN && error != EWOULDBLOCK) {
            return -error;
        }

        stream_pollfd_t poll_fd = {
            .fd = fd,
            .events = POLLOUT,
        };
        int activity = STREAM_POLL(&poll_fd, 1, 10);
        if (activity == 0) {
            return -EAGAIN;
        } else if (activity < 0) {
            return -stream_socket_errno();
        }

        int       native_error = 0;
        socklen_t error_length = sizeof(native_error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&native_error,
                &error_length) == SOCKET_ERROR) {
            return -stream_socket_errno();
        } else if (native_error != 0) {
            return -stream_socket_error_from_native(native_error);
        }
    }

    return stream_socket_set_blocking(fd);
}


static inline int32_t stream_socket_connect_retry(stream_socket_t* fd,
    sa_family_t family, const struct sockaddr* addr, socklen_t addr_len)
{
    ULONGLONG connect_deadline = GetTickCount64() + 5000U;
    while (GetTickCount64() < connect_deadline) {
        *fd = socket(family, SOCK_STREAM, 0);
        if (!STREAM_SOCKET_IS_VALID(*fd)) {
            return -stream_socket_errno();
        }

        int32_t rc = stream_socket_connect_once(*fd, addr, addr_len);
        if (rc == 0) {
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
    if (poll_fd == NULL ||
        !(poll_fd->revents & (POLLERR | POLLHUP | POLLNVAL))) {
        return 0;
    }

    int       native_error = 0;
    socklen_t error_length = sizeof(native_error);
    if (getsockopt(poll_fd->fd, SOL_SOCKET, SO_ERROR, (char*)&native_error,
            &error_length) == SOCKET_ERROR) {
        return -stream_socket_errno();
    } else if (native_error != 0) {
        return -stream_socket_error_from_native(native_error);
    } else {
        return -EIO;
    }
}


static inline int stream_configure_socket_timeout(stream_socket_t fd)
{
    DWORD timeout_ms = 1000;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms,
            sizeof(timeout_ms)) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms,
            sizeof(timeout_ms)) == SOCKET_ERROR) {
        return -stream_socket_errno();
    }

    return 0;
}


static inline int32_t stream_getaddrinfo_error(int rc)
{
    switch (rc) {
#ifdef EAI_AGAIN
    case EAI_AGAIN:
        return -EAGAIN;
#endif

#ifdef EAI_MEMORY
    case EAI_MEMORY:
        return -ENOMEM;
#endif

#ifdef EAI_NONAME
    case EAI_NONAME:
        return -EADDRNOTAVAIL;
#endif

#ifdef EAI_FAIL
    case EAI_FAIL:
        return -EIO;
#endif

#ifdef EAI_FAMILY
    case EAI_FAMILY:
        return -EAFNOSUPPORT;
#endif

#ifdef EAI_SERVICE
    case EAI_SERVICE:
        return -EPROTONOSUPPORT;
#endif

#ifdef EAI_SOCKTYPE
    case EAI_SOCKTYPE:
        return -EPROTONOSUPPORT;
#endif

    default:
        return -EINVAL;
    }
}

#else
#error "stream_win.h must only be included on Windows"
#endif

#endif  // DSE_MODELC_ADAPTER_TRANSPORT_STREAM_WIN_H_
