// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_ADAPTER_PRIVATE_H_
#define DSE_MODELC_ADAPTER_ADAPTER_PRIVATE_H_


#include <stdint.h>
#include <dse/modelc/adapter/message.h>
#include <dse_schemas/flatbuffers/simbus_channel_builder.h>
#include <dse/platform.h>


typedef struct AdapterPrivate {
    /* Properties supporting the operation of the Adapter.*/
    flatcc_builder_t builder;

    /* Message handling. */
    HandleChannelMessageFunc handle_message;
    HandleNotifyMessageFunc  handle_notify_message;

    /* Transport Interface. */
    uint8_t* ep_buffer;
    uint32_t ep_buffer_length;
} AdapterPrivate;


/* adapter.c */
DLL_PRIVATE Channel* _get_channel(AdapterModel* am, const char* channel_name);
DLL_PRIVATE Channel* _get_channel_byindex(AdapterModel* am, uint32_t index);
DLL_PRIVATE SignalValue* _find_signal_by_uid(Channel* channel, uint32_t uid);
DLL_PRIVATE void         _update_signal_value_keys(Channel* channel);
DLL_PRIVATE SignalValue* _get_signal_value(
    Channel* channel, const char* signal_name);
DLL_PRIVATE SignalValue* _get_signal_value_byindex(
    Channel* channel, uint32_t index);


#endif  // DSE_MODELC_ADAPTER_ADAPTER_PRIVATE_H_
