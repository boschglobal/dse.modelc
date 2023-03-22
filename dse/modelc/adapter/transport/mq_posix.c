// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/mq.h>

#define UNUSED(x) ((void)x)

#ifndef _WIN32
#include <mqueue.h>


/**
 * MQ POSIX Integration
 * ====================
 *
 * Reference:
 * http://manpages.ubuntu.com/manpages/focal/en/man7/mq_overview.7.html
 *
 * Linux "/proc" Interfaces
 * ------------------------
 * Before using this transport ensure that the following /proc settings are at
 * least as large as the defines in this file.
 *
 * mq_maxmsg : (MQ_POSIX_MAXMSG)
 *      /proc/sys/fs/mqueue/msg_max (default 10)
 *
 * mq_msgsize : (MQ_POSIX_MSGSIZE)
 *      /proc/sys/fs/mqueue/msgsize_max (default 8192)
 */


#define MQ_POSIX_OFLAG_PUSH    (O_CREAT | O_RDWR | O_NONBLOCK)
#define MQ_POSIX_OFLAG_PULL    (O_CREAT | O_RDWR)
#define MQ_POSIX_MASK          0644
#define MQ_POSIX_PRIO          8
#define MQ_POSIX_MAXMSG_SIMBUS 50
#define MQ_POSIX_MAXMSG_MODEL  4
#define MQ_POSIX_MSGSIZE       MQ_MAX_MSGSIZE
#define MQ_POSIX_ATTR_SIMBUS                                                   \
    ((struct mq_attr){ 0, MQ_POSIX_MAXMSG_SIMBUS, MQ_POSIX_MSGSIZE, 0, { 0 } })
#define MQ_POSIX_ATTR_MODEL                                                    \
    ((struct mq_attr){ 0, MQ_POSIX_MAXMSG_MODEL, MQ_POSIX_MSGSIZE, 0, { 0 } })


static int __stop_request = 0;


DLL_PRIVATE void mq_posix_open(MqDesc* mq_desc, MqKind kind, MqMode mode)
{
    assert(mq_desc);
    struct mq_attr _attr =
        (kind == MQ_KIND_MODEL) ? MQ_POSIX_ATTR_MODEL : MQ_POSIX_ATTR_SIMBUS;
    mode_t _mode =
        (mode == MQ_MODE_PULL) ? MQ_POSIX_OFLAG_PULL : MQ_POSIX_OFLAG_PUSH;

    if (mq_desc->mqd > 0) return;

    mq_desc->mqd = mq_open(mq_desc->endpoint, _mode, MQ_POSIX_MASK, &_attr);
    if (mq_desc->mqd == -1) {
        log_fatal("Could not open endpoint: %s", mq_desc->endpoint);
    }
}


DLL_PRIVATE int mq_posix_send(MqDesc* mq_desc, char* buffer, size_t len)
{
    assert(mq_desc);
    assert(mq_desc->mqd);

    return mq_send(mq_desc->mqd, buffer, len, MQ_POSIX_PRIO);
}


DLL_PRIVATE int mq_posix_recv(
    MqDesc* mq_desc, char* buffer, size_t len, struct timespec* tm)
{
    assert(mq_desc);
    assert(mq_desc->mqd);

    return mq_timedreceive(mq_desc->mqd, buffer, len, NULL, tm);
}


DLL_PRIVATE void mq_posix_interrupt(void)
{
    __stop_request = 1;
}


DLL_PRIVATE void mq_posix_unlink(MqDesc* mq_desc)
{
    assert(mq_desc);
    mq_unlink(mq_desc->endpoint);
}


DLL_PRIVATE void mq_posix_close(MqDesc* mq_desc)
{
    assert(mq_desc);
    if (mq_desc->mqd > 0) mq_close(mq_desc->mqd);
    mq_desc->mqd = 0;
}


void mq_posix_configure(Endpoint* endpoint)
{
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    mq_ep->mq_open = mq_posix_open;
    mq_ep->mq_send = mq_posix_send;
    mq_ep->mq_recv = mq_posix_recv;
    mq_ep->mq_interrupt = mq_posix_interrupt;
    mq_ep->mq_unlink = mq_posix_unlink;
    mq_ep->mq_close = mq_posix_close;
}
#endif  // #ifndef _WIN32

#ifdef _WIN32
void mq_posix_configure(Endpoint* endpoint)
{
    UNUSED(endpoint);
    log_fatal("Posix MQ not supported on this platform (%s-%s)", PLATFORM_OS,
        PLATFORM_ARCH);
}
#endif
