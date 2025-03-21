// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_PRIVATE_H_
#define DSE_MODELC_ADAPTER_PRIVATE_H_


#include <stdint.h>
#include <msgpack.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/platform.h>


/* index.c */
DLL_PRIVATE void _refresh_index(Channel* channel);
DLL_PRIVATE void _destroy_index(Channel* channel);
DLL_PRIVATE void _invalidate_index(Channel* channel);
DLL_PRIVATE void _generate_index(Channel* channel);

DLL_PRIVATE Channel* _get_channel(AdapterModel* am, const char* channel_name);
DLL_PRIVATE Channel* _get_channel_byindex(AdapterModel* am, uint32_t index);

DLL_PRIVATE SignalValue* _find_signal_by_uid(Channel* channel, uint32_t uid);
DLL_PRIVATE SignalValue* _get_signal_value(
    Channel* channel, const char* signal_name);
DLL_PRIVATE SignalValue* _get_signal_value_byindex(
    Channel* channel, uint32_t index);
DLL_PRIVATE SignalMap* _get_signal_value_map(
    Channel* channel, const char** signal_name, uint32_t signal_count);

/* msgpack.c*/
DLL_PRIVATE void sv_delta_to_msgpack(
    Channel* channel, msgpack_packer* pk, const char* log_prefix);
DLL_PRIVATE void sv_delta_to_msgpack_binonly(
    Channel* channel, msgpack_packer* pk, const char* log_prefix);


#endif  // DSE_MODELC_ADAPTER_PRIVATE_H_
