// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TRANSPORT_MQ_H_
#define DSE_MODELC_ADAPTER_TRANSPORT_MQ_H_

/**
 *  The Message Queue transport supports several possible message queue
 *  implementations which are selected by the `uri` specifier.
 *
 *  Example CLI
 *  -----------
 *
 *      $ modelc --transport mq --uri posix
 *
 *  Example Stack
 *  -------------
 *
 *      ---
 *      kind: Stack
 *      metadata:
 *        name: SimBus Stack
 *      spec:
 *        connection:
 *          transport:
 *            mq:
 *              uri: posix
 */

#include <stdint.h>
#include <time.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/platform.h>

#ifndef _WIN32
#include <mqueue.h>
#endif


#define MQ_MAX_EP_LEN  100
#define MQ_MAX_MSGSIZE 65536


typedef enum MqScheme {
    MQ_SCHEME_POSIX = 0,     /* Posix MQ based IPC */
    MQ_SCHEME_NAMEDPIPE = 1, /* Named Pipe MQ based IPC */
    /* Count of all actions. */
    __MQ_SCHEME_COUNT__
} MqScheme;

typedef enum MqMode {
    MQ_MODE_PUSH = 0,
    MQ_MODE_PULL,
    /* Count of all modes. */
    __MQ_MODE_COUNT__
} MqMode;

typedef enum MqKind {
    MQ_KIND_SIMBUS = 0,
    MQ_KIND_MODEL,
    /* Count of all modes. */
    __MQ_KIND_COUNT__
} MqKind;

typedef struct MqChannel {
    const char* name;
    /* Reference to the Adapter Channel linked to this Endpoint. */
    void*       adapter_channel;
} MqChannel;

typedef struct MqDesc {
    char endpoint[MQ_MAX_EP_LEN];
/* Posix MQ. */
#ifndef _WIN32
    mqd_t mqd;
#endif
/* Windows Namedpipe . */
#ifdef _WIN32
    HANDLE     hPipe;
    OVERLAPPED oOverlap;
    BOOL       fPendingIO;
    DWORD      dwState;
#endif
} MqDesc;

typedef void (*MqOpenFunc)(MqDesc* mq_desc, MqKind kind, MqMode mode);
typedef int (*MqSendFunc)(MqDesc* mq_desc, char* buffer, size_t len);
typedef int (*MqRecvFunc)(
    MqDesc* mq_desc, char* buffer, size_t len, struct timespec* tm);
typedef void (*MqInterruptFunc)(void);
typedef void (*MqUnlinkFunc)(MqDesc* mq_desc);
typedef void (*MqCloseFunc)(MqDesc* mq_desc);

typedef struct MqEndpoint {
    /* Message Queue properties. */
    const char*     scheme;
    char*           stem;
    double          recv_timeout;
    /* MQ Interface. */
    MqOpenFunc      mq_open;
    MqSendFunc      mq_send;
    MqRecvFunc      mq_recv;
    MqInterruptFunc mq_interrupt;
    MqUnlinkFunc    mq_unlink;
    MqCloseFunc     mq_close;
    /* Endpoints. */
    HashMap         push_hash; /* Used in bus_mode, push to Models. */
    MqDesc          push;
    MqDesc          pull;
} MqEndpoint;


/* mq.c */
DLL_PRIVATE const char* mq_scheme(MqScheme scheme);
DLL_PRIVATE Endpoint*   mq_connect(
      const char* uri, uint32_t model_uid, bool bus_mode, double recv_timeout);
DLL_PRIVATE MqDesc* mq_model_push_desc(Endpoint* endpoint, uint32_t model_uid);
DLL_PRIVATE void    mq_endpoint_destroy(Endpoint* endpoint);
DLL_PRIVATE void*   mq_create_channel(
      Endpoint* endpoint, const char* channel_name, void* adapter_channel);
DLL_PRIVATE int32_t mq_start(Endpoint* endpoint);
DLL_PRIVATE void    mq_interrupt(Endpoint* endpoint);
DLL_PRIVATE void    mq_disconnect(Endpoint* endpoint);
DLL_PRIVATE int32_t mq_send_fbs(Endpoint* endpoint, void* endpoint_channel,
    void* buffer, uint32_t buffer_length, uint32_t model_uid);
DLL_PRIVATE int32_t mq_recv_fbs(Endpoint* endpoint, void** adapter_channel,
    uint8_t** buffer, uint32_t* buffer_length);


#ifndef _WIN32
/* mq_posix.c */
DLL_PUBLIC void mq_posix_configure(Endpoint* endpoint);
#else
/* mq_namedpipe.c */
DLL_PUBLIC void mq_namedpipe_configure(Endpoint* endpoint);
#endif


#endif  // DSE_MODELC_ADAPTER_TRANSPORT_MQ_H_
