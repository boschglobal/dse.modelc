// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/mq.h>


#define UNUSED(x)        ((void)x)


/**
 * MQ namedpipe Integration for Windows
 * ====================
 */

#define BUFSIZE          512
#define PIPE_TIMEOUT     500

#define INSTANCES        PIPE_UNLIMITED_INSTANCES
#define CONNECTING_STATE 0
#define READING_STATE    1
#define WRITING_STATE    2


typedef struct MqHandle {
    HANDLE     hPipe;
    OVERLAPPED oOverlap;
    BOOL       fPendingIO;
    DWORD      dwState;
} MqHandle;


static int __stop_request = 0;

static readCompleteBytes = 0;
static writeCompleteBytes = 0;

// If set this time to 200 - dynamic model example works like expected
static sleepTime = 0;

VOID WINAPI readComplete(DWORD err, DWORD bytes, LPOVERLAPPED ovlp)
{
    printf("MQ_NAMEDPIPE: readComplete  err=%d  bytes=%d\n", err, bytes);
    readCompleteBytes = bytes;
}

VOID WINAPI writeComplete(DWORD err, DWORD bytes, LPOVERLAPPED ovlp)
{
    printf("MQ_NAMEDPIPE: writeComplete  err=%d  bytes=%d\n", err, bytes);
    writeCompleteBytes = bytes;
}

// ConnectToNewClient(HANDLE, LPOVERLAPPED)
// This function is called to start an overlapped connect operation
// It returns TRUE if an operation is pending or FALSE if the connection has
// been completed
BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
    BOOL fConnected, fPendingIO = FALSE;
    // Start an overlapped connection for this pipe instance
    fConnected = ConnectNamedPipe(hPipe, lpo);
    // Overlapped ConnectNamedPipe should return zero
    if (fConnected) {
        log_debug(
            "MQ_NAMEDPIPE: ConnectNamedPipe() failed with error code %d.\n",
            GetLastError());
        return 0;
    } else
        log_debug("MQ_NAMEDPIPE: ConnectNamedPipe() successfully.\n");

    switch (GetLastError()) {
    // The overlapped connection in progress
    case ERROR_IO_PENDING:
        fPendingIO = TRUE;
        log_debug("MQ_NAMEDPIPE: fPendingIO = TRUE\n");
        break;
    // Client is already connected, so signal an event
    case ERROR_PIPE_CONNECTED:
        if (SetEvent(lpo->hEvent)) {
            log_debug(
                "MQ_NAMEDPIPE: SetEvent successfully in ConnectToNewClient\n");
            break;
        } else {
            log_debug("MQ_NAMEDPIPE: SetEvent failed in ConnectToNewClient\n");
        }
    // If an error occurs during the connect operation...
    default: {
        log_debug(
            "MQ_NAMEDPIPE: ConnectNamedPipe() failed with error code %d.\n",
            GetLastError());
        return 0;
    }
    }
    return fPendingIO;
}


DLL_PRIVATE void mq_namedpipe_open(MqDesc* mq_desc, MqKind kind, MqMode mode)
{
    log_debug("MQ_NAMEDPIPE: CreateNamedPipe processID=%d, %s, %d, %d",
        GetCurrentProcessId(), mq_desc->endpoint, kind, mode);
    assert(mq_desc);

    if (mq_desc->hPipe) return;

    HANDLE hNamedPipe = INVALID_HANDLE_VALUE;

    char szPipeName[256];
    snprintf(
        szPipeName, sizeof(szPipeName), "\\\\.\\pipe\\%s", mq_desc->endpoint);
    LPCTSTR lpszPipename = TEXT(szPipeName);
    log_debug("MQ_NAMEDPIPE: pipe name %s", szPipeName);


    // If pull -> create namedpipe server
    if (mode == MQ_MODE_PULL) {
        log_debug("MQ_NAMEDPIPE: create server");

        BOOL       fConnected = FALSE;
        OVERLAPPED oOverlap;
        HANDLE     hEvent;

        hNamedPipe = CreateNamedPipe(lpszPipename,  // pipe name
            PIPE_ACCESS_DUPLEX |                    // read/write access
                FILE_FLAG_OVERLAPPED,               // overlapped mode
            PIPE_TYPE_MESSAGE |                     // message type pipe
                PIPE_READMODE_MESSAGE |             // message-read mode
                PIPE_WAIT,                          // blocking mode
            PIPE_UNLIMITED_INSTANCES,               // max. instances
            BUFSIZE * sizeof(TCHAR),                // output buffer size
            BUFSIZE * sizeof(TCHAR),                // input buffer size
            PIPE_TIMEOUT,                           // client time-out
            NULL);  // default security attribute
        log_debug("MQ_NAMEDPIPE: CreateNamedPipe hNamedPipe=%d, pipe_name='%s'",
            hNamedPipe, lpszPipename);

        if (hNamedPipe == INVALID_HANDLE_VALUE) {
            log_notice("MQ_NAMEDPIPE: INVALID CreateNamedPipe");
            log_fatal(
                "CreateNamedPipe failed with error code %d.\n", GetLastError());
        } else {
            log_debug("MQ_NAMEDPIPE: CreateNamedPipe successfully");
            mq_desc->hPipe = hNamedPipe;
        }

        hEvent = CreateEvent(NULL,  // default security attribute
            TRUE,                   // manual-reset event
            TRUE,                   // initial state = signaled
            NULL);                  // unnamed event object
        if (hEvent == NULL) {
            log_fatal("CreateEvent(hEvent) failed with error code %d.\n",
                GetLastError());
            return;
        } else {
            log_debug("CreateEvent() successfully!\n");
        }

        // SecureZeroMemory(&overlapped, sizeof(overlapped));
        oOverlap.hEvent = hEvent;
        mq_desc->oOverlap = oOverlap;

        BOOL fPendingIO =
            ConnectToNewClient(mq_desc->hPipe, &mq_desc->oOverlap);
        mq_desc->fPendingIO = fPendingIO;

        DWORD dwState;
        dwState = fPendingIO ? CONNECTING_STATE :  // still connecting
                      READING_STATE;
        mq_desc->dwState = dwState;
        log_debug("MQ_NAMEDPIPE: dwState = %d\n", dwState);
    }

    // If pull -> create namedpipe client
    if (mode == MQ_MODE_PUSH) {
        log_notice("MQ_NAMEDPIPE: create client");

        hNamedPipe = CreateFile(lpszPipename,  // pipe name
            GENERIC_READ |                     // read and write access
                GENERIC_WRITE,
            0,                     // no sharing
            NULL,                  // default security attribute
            OPEN_EXISTING,         // opens existing pipe
            FILE_FLAG_OVERLAPPED,  // default attributes
            NULL);                 // no template file


        // Break if the pipe handle is valid
        if (hNamedPipe == INVALID_HANDLE_VALUE) {
            // Exit if an error other than ERROR_PIPE_BUSY occurs
            if (GetLastError() != ERROR_PIPE_BUSY) {
                log_debug("MQ_NAMEDPIPE: Could not open pipe! ERROR_PIPE_BUSY");
                return;
            }
            // All pipe instances are busy, so wait for 20 seconds
            if (!WaitNamedPipe(lpszPipename, 20000)) {
                log_debug(
                    "MQ_NAMEDPIPE: Could not open pipe! After waiting 20s");
                return;
            }
        }
        mq_desc->hPipe = hNamedPipe;
        log_debug(
            "MQ_NAMEDPIPE: Pipe was opened and connected successfully!\n");
    }
}


DLL_PRIVATE int mq_namedpipe_send(MqDesc* mq_desc, char* buffer, size_t len)
{
    log_debug("\n\nMQ_NAMEDPIPE: mq_namedpipe_send processID=%d, %s, "
              "buffer=%s, len=%d",
        GetCurrentProcessId(), mq_desc->endpoint, buffer, len);
    assert(mq_desc);
    assert(mq_desc->hPipe);

    BOOL  fSuccess, fSuccessGetOvResult = FALSE;
    DWORD cbRet, dwErr;

    log_debug("MQ_NAMEDPIPE: before WriteFileEx");

    fSuccess = WriteFileEx(mq_desc->hPipe,  // handle to pipe
        buffer,                             // buffer to write from
        len * sizeof(TCHAR),                // number of bytes to write
        &mq_desc->oOverlap,                 // OVERLAPPED data structure
        (LPOVERLAPPED_COMPLETION_ROUTINE)writeComplete);  // overlapped I/O

    /*
     if (HasOverlappedIoCompleted(&mq_desc->oOverlap)) {
       puts("Completed WRITE");
     } else {
       puts("Not completed WRITE");
     }
     */

    Sleep(sleepTime);

    log_debug("MQ_NAMEDPIPE: WRITE GetOverlappedResult");
    fSuccessGetOvResult = GetOverlappedResult(mq_desc->hPipe,  // handle to pipe
        &mq_desc->oOverlap,  // OVERLAPPED data structure
        &cbRet,              // bytes transferred
        FALSE);              // do not wait
    log_debug(
        "MQ_NAMEDPIPE: WRITE GetOverlappedResult transferred_bytes=%d", cbRet);
    /*
    log_notice("MQ_NAMEDPIPE: WRITE GetOverlappedResultEx");
                BOOL fSuccessGetOvResultEx = GetOverlappedResultEx(
                        mq_desc->hPipe,			      // handle to pipe
                        &mq_desc->oOverlap,			  // OVERLAPPED
    data structure &cbRet,                   // bytes transferred 60000, //
    time-out interval, in milliseconds. TRUE);                    // wait
                log_notice("MQ_NAMEDPIPE: WRITE GetOverlappedResultEx
    transferred_bytes=%d",cbRet);

    while (true)
    {
      dwErr = GetLastError();
      switch (dwErr)
        {
                // The overlapped connection in progress
                case WAIT_IO_COMPLETION:
        {
          log_debug("MQ_NAMEDPIPE: WRITE GetOverlappedResultEx -
    WAIT_IO_COMPLETION \n"); break;
        }
                case ERROR_IO_INCOMPLETE:
        {
          log_debug("MQ_NAMEDPIPE: WRITE GetOverlappedResultEx -
    ERROR_IO_INCOMPLETE \n"); break;
        }
        case WAIT_TIMEOUT:
        {
          log_debug("MQ_NAMEDPIPE: WRITE GetOverlappedResultEx - WAIT_TIMEOUT
    \n"); break;
        }
                default:
                {
                        log_debug("MQ_NAMEDPIPE: WRITE GetOverlappedResultEx
    error %d.\n", GetLastError());
                }
        }

      if (dwErr == WAIT_IO_COMPLETION)
      {
        break;
      }
    }
    */

    // if (fSuccess) return 0; else return -1;
    if (fSuccess) {
        log_debug("MQ_NAMEDPIPE: WRITE successfully, message=%s", buffer);
        return 0;
    } else {
        log_debug(
            "MQ_NAMEDPIPE: WRITE failed, GetLastError=%d", GetLastError());
        return -1;
    }
}


DLL_PRIVATE int mq_namedpipe_recv(
    MqDesc* mq_desc, char* buffer, size_t len, struct timespec* tm)
{
    log_debug("\n\nOMQ_NAMEDPIPE: mq_namedpipe_recv processID=%d, %s, "
              "buffer=%s, len=%d",
        GetCurrentProcessId(), mq_desc->endpoint, buffer, len);
    assert(mq_desc);
    assert(mq_desc->hPipe);
    assert(mq_desc->oOverlap);
    assert(mq_desc->dwState);

    BOOL  fSuccessRead, fSuccessGetOvResult = FALSE;
    DWORD cbRet, dwErr, ret;
    DWORD cbTotalBytesAvail, cbBytesLeftThisMessage;

    log_debug("MQ_NAMEDPIPE: before ReadFileEx");
    fSuccessRead = ReadFileEx(mq_desc->hPipe,  // handle to pipe
        buffer,                                // buffer to receive data
        len * sizeof(TCHAR),                   // size of buffer
        &mq_desc->oOverlap,                    // overlapped I/O
        (LPOVERLAPPED_COMPLETION_ROUTINE)readComplete);

    /*
    if (HasOverlappedIoCompleted(&mq_desc->oOverlap)) {
      puts("Completed READ");
    } else {
      puts("Not completed READ");
    }
    s*/

    Sleep(sleepTime);

    log_debug("MQ_NAMEDPIPE: READ GetOverlappedResult");
    fSuccessGetOvResult = GetOverlappedResult(mq_desc->hPipe,  // handle to pipe
        &mq_desc->oOverlap,  // OVERLAPPED data structure
        &cbRet,              // bytes transferred
        TRUE);               // do not wait
    log_debug(
        "MQ_NAMEDPIPE: READ GetOverlappedResult transferred_bytes=%d", cbRet);
    if (!fSuccessGetOvResult) {
        dwErr = GetLastError();
        log_debug("MQ_NAMEDPIPE: Error %d GetOverlappedResult!\n", dwErr);
        if (dwErr == ERROR_BROKEN_PIPE || dwErr == ERROR_INVALID_HANDLE) {
            mq_namedpipe_interrupt();
            mq_namedpipe_unlink(&mq_desc);
            mq_namedpipe_close(&mq_desc);
        }
    }

    /*
    log_notice("MQ_NAMEDPIPE: READ GetOverlappedResultEx");
              BOOL fSuccessGetOvResultEx = GetOverlappedResultEx(
                      mq_desc->hPipe,			      // handle to pipe
                      &mq_desc->oOverlap,			  // OVERLAPPED
    data structure &cbRet,                   // bytes transferred 60000, //
    time-out interval, in milliseconds. TRUE);                    // If this
    parameter is TRUE and the calling thread is in the waiting state,
                                // the function returns when the system queues
    an I/O completion routine or APC. log_notice("MQ_NAMEDPIPE: READ
    GetOverlappedResultEx transferred_bytes=%d",cbRet);

    while (true)
    {
      dwErr = GetLastError();
      switch (dwErr)
      {
              // The overlapped connection in progress
              case WAIT_IO_COMPLETION:
        {
          log_debug("MQ_NAMEDPIPE: READ GetOverlappedResultEx -
    WAIT_IO_COMPLETION \n"); break;
        }
              case ERROR_IO_INCOMPLETE:
        {
          log_debug("MQ_NAMEDPIPE: READ GetOverlappedResultEx -
    ERROR_IO_INCOMPLETE \n"); break;
        }
        case WAIT_TIMEOUT:
        {
          log_debug("MQ_NAMEDPIPE: READ GetOverlappedResultEx - WAIT_TIMEOUT
    \n"); break;
        }
              default:
              {
                      log_debug("MQ_NAMEDPIPE: READ GetOverlappedResultEx error
    %d.\n", GetLastError()); break;
              }
      }

      if (dwErr == WAIT_IO_COMPLETION)
      {
        break;
      }
      else if(dwErr!=ERROR_IO_INCOMPLETE)
      {
        break;
        return 0;
      }
    }
    */

    ret = cbRet;
    log_debug("MQ_NAMEDPIPE: READ ret=%d", ret);
    return ret;
}


DLL_PRIVATE void mq_namedpipe_interrupt(void)
{
    __stop_request = 1;
}


DLL_PRIVATE void mq_namedpipe_unlink(MqDesc* mq_desc)
{
    assert(mq_desc);
    assert(mq_desc->hPipe);

    FlushFileBuffers(mq_desc->hPipe);
    DisconnectNamedPipe(mq_desc->hPipe);
}


DLL_PRIVATE void mq_namedpipe_close(MqDesc* mq_desc)
{
    assert(mq_desc);
    assert(mq_desc->hPipe);
    CloseHandle(mq_desc->hPipe);
    mq_desc->hPipe = NULL;
}


void mq_namedpipe_configure(Endpoint* endpoint)
{
    log_notice("MQ_NAMEDPIPE: begin mq_namedpipe_configure(endpoint)");
    assert(endpoint);
    assert(endpoint->private);
    MqEndpoint* mq_ep = (MqEndpoint*)endpoint->private;

    mq_ep->mq_open = mq_namedpipe_open;
    mq_ep->mq_send = mq_namedpipe_send;
    mq_ep->mq_recv = mq_namedpipe_recv;
    mq_ep->mq_interrupt = mq_namedpipe_interrupt;
    mq_ep->mq_unlink = mq_namedpipe_unlink;
    mq_ep->mq_close = mq_namedpipe_close;
    log_notice("MQ_NAMEDPIPE: end mq_namedpipe_configure(endpoint)");
}
