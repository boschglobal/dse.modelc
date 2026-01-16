// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_MESSAGE_H_
#define DSE_MODELC_ADAPTER_MESSAGE_H_

#include <stdint.h>
#include <dse_schemas/flatbuffers/simbus_channel_builder.h>
#include <dse_schemas/flatbuffers/simbus_notify_builder.h>
#include <dse/platform.h>


#undef flatbuffers_identifier
#define flatbuffers_notify_identifier "SBNO"
#undef notify
#define notify(x) FLATBUFFERS_WRAP_NAMESPACE(dse_schemas_fbs_notify, x)


/* Adapter Message Interface. */

typedef void (*HandleNotifyMessageFunc)(
    Adapter* adapter, notify(NotifyMessage_table_t) notify_message);

typedef struct AdapterMsgVTable {
    AdapterVTable vtable;

    /* Message handling. */
    HandleNotifyMessageFunc handle_notify_message;

    /* Supporting data objects. */
    flatcc_builder_t builder;
    uint8_t*         ep_buffer;
    uint32_t         ep_buffer_length;
} AdapterMsgVTable;


/* message.c */
DLL_PRIVATE int32_t send_notify_message(
    Adapter* adapter, uint32_t model_uid, notify(NotifyMessage_ref_t) message);
DLL_PRIVATE int32_t send_notify_message_wait_ack(Adapter* adapter,
    uint32_t model_uid, notify(NotifyMessage_ref_t) message, int32_t token);
DLL_PRIVATE int32_t wait_message(
    Adapter* adapter, const char** channel_name, int32_t token, bool* found);


#endif  // DSE_MODELC_ADAPTER_MESSAGE_H_
