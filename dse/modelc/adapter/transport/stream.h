// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_STREAM_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_STREAM_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


#define STREAM_MAX_MESSAGE_LENGTH             (64U * 1024U * 1024U)
#define STREAM_SOCKET_BUFFER_LENGTH           (STREAM_MAX_MESSAGE_LENGTH / 2U)
#define STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS 18


#ifdef _WIN32
#include <dse/modelc/adapter/transport/stream_win.h>
#else
#include <dse/modelc/adapter/transport/stream_linux.h>
#endif
#include <dse/clib/collections/vector.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/platform.h>


typedef struct StreamMessage {
    stream_socket_t fd; /* Identifying property. */
    uint8_t*        data;
    uint32_t        length;
} StreamMessage;


typedef struct StreamInstance {
    stream_socket_t fd;
    uint32_t        uid;

    /* Incremental receive buffer. */
    struct {
        uint8_t* data;
        uint32_t length;
        uint32_t capacity;
    } recv_buffer;
} StreamInstance;


typedef struct StreamEndpoint {
    const char*             uri;
    struct sockaddr_storage addr;
    socklen_t               addr_len;

    /* Active stream connections. */
    struct {
        StreamInstance simbus;
        Vector         models;      /* StreamInstance */
        Vector         poll_fds;    /* struct stream_pollfd_t */
        Vector         rx_messages; /* StreamMessage */

        /* Diagnostic counters. */
        struct {
            uint64_t rx_poll_count;
            uint64_t rx_cached_returns;
            uint64_t rx_recv_calls;
            uint64_t rx_messages_queued;
        } diagnostics;
    } server;
    struct {
        StreamInstance model;
    } client;

    /* RX properties. */
    double recv_timeout;

    struct {
        uint64_t tx[STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS];
        uint64_t rx[STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS];
    } message_size_histogram;
} StreamEndpoint;


static inline size_t stream_message_size_histogram_bucket(
    uint32_t message_length)
{
    size_t   bucket = 0;
    uint64_t upper_bound = 1024U;

    while (message_length > upper_bound &&
           bucket + 1 < STREAM_MESSAGE_SIZE_HISTOGRAM_BUCKETS) {
        upper_bound <<= 1U;
        bucket++;
    }

    return bucket;
}


/* stream.c */
DLL_PRIVATE Endpoint* stream_connect(
    const char* uri, uint32_t uid, bool bus_mode, double timeout);
void* stream_create_channel(Endpoint* endpoint, const char* channel_name);
void  stream_interrupt(Endpoint* endpoint);


/* stream_posix.c (POSIX Implementation). */
int32_t stream_posix_start(Endpoint* endpoint);
int32_t stream_posix_send_fbs(Endpoint* endpoint, void* endpoint_channel,
    void* buffer, uint32_t buffer_length, uint32_t model_uid);
int32_t stream_posix_recv_fbs(Endpoint* endpoint, const char** channel_name,
    uint8_t** buffer, uint32_t* buffer_length);
void    stream_posix_disconnect(Endpoint* endpoint);


/* Internal helpers. */
DLL_PRIVATE void    stream_instance_free_recv_buffer(StreamInstance* si);
DLL_PRIVATE int32_t stream_queue_rx_message(StreamEndpoint* ep,
    stream_socket_t fd, const uint8_t* data, uint32_t length);
DLL_PRIVATE int32_t stream_pop_rx_message(
    StreamEndpoint* ep, uint8_t** buffer, uint32_t* buffer_length);
DLL_PRIVATE int32_t stream_instance_reserve_recv_buffer(
    StreamInstance* si, uint32_t needed);
DLL_PRIVATE uint64_t stream_time_ns(void);
DLL_PRIVATE void     stream_endpoint_destroy(Endpoint* endpoint);


#endif  // DSE_MODELC_ADAPTER_TRANSPORT_STREAM_H_
