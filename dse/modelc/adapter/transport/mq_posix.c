// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/mq.h>


#define UNUSED(x)           ((void)x)


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
 * least as large as required for the simulation.
 *
 *      /proc/sys/fs/mqueue/msg_default (default 10)
 *      /proc/sys/fs/mqueue/msgsize_default (default 8192)
 */


#define MQ_POSIX_OFLAG_PUSH (O_CREAT | O_RDWR | O_NONBLOCK)
#define MQ_POSIX_OFLAG_PULL (O_CREAT | O_RDWR)
#define MQ_POSIX_MASK       0644
#define MQ_POSIX_PRIO       8


typedef struct MqHandle {
    mqd_t mqd;
} MqHandle;


static int __stop_request = 0;


DLL_PRIVATE void mq_posix_open(MqDesc* mq_desc, MqKind kind, MqMode mode)
{
    UNUSED(kind);
    assert(mq_desc);
    if (mq_desc->data == NULL) {
        mq_desc->data = calloc(1, sizeof(MqHandle));
    }
    MqHandle* handle = mq_desc->data;
    if (handle->mqd > 0) return;

    mode_t _mode =
        (mode == MQ_MODE_PULL) ? MQ_POSIX_OFLAG_PULL : MQ_POSIX_OFLAG_PUSH;
    handle->mqd = mq_open(mq_desc->endpoint, _mode, MQ_POSIX_MASK, NULL);
    if (handle->mqd == -1) {
        log_error("Could not open endpoint, check configuration: sysctl "
                  "fs.mqueue and ulimit -a !");
        log_fatal("Could not open endpoint: %s", mq_desc->endpoint);
    }
}


DLL_PRIVATE int mq_posix_send(MqDesc* mq_desc, char* buffer, size_t len)
{
    assert(mq_desc);
    assert(mq_desc->data);
    MqHandle* handle = mq_desc->data;
    assert(handle->mqd);

    return mq_send(handle->mqd, buffer, len, MQ_POSIX_PRIO);
}


DLL_PRIVATE int mq_posix_recv(
    MqDesc* mq_desc, char* buffer, size_t len, struct timespec* tm)
{
    assert(mq_desc);
    assert(mq_desc->data);
    MqHandle* handle = mq_desc->data;
    assert(handle->mqd);

    return mq_timedreceive(handle->mqd, buffer, len, NULL, tm);
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
    if (mq_desc->data == NULL) return;

    MqHandle* handle = mq_desc->data;
    if (handle->mqd > 0) mq_close(handle->mqd);
    handle->mqd = 0;
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
