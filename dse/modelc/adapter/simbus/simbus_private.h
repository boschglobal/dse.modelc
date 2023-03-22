// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_PRIVATE_H_
#define DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_PRIVATE_H_


#include <stdbool.h>
#include <stdint.h>
#include <dse_schemas/flatbuffers/simbus_channel_builder.h>
#include <dse/platform.h>
#include <dse/modelc/adapter/adapter.h>


#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(dse_schemas_fbs_channel, x)


/* adapter.c */
DLL_PRIVATE uint32_t simbus_generate_uid_hash(const char* key);


/* handler.c */
DLL_PRIVATE void simbus_handle_message(Adapter* adapter,
    const char* channel_name, ns(ChannelMessage_table_t) channel_message,
    int32_t     token);


/* states.c */
DLL_PRIVATE bool simbus_network_ready(AdapterModel* am);
DLL_PRIVATE bool simbus_models_ready(AdapterModel* am);
DLL_PRIVATE void simbus_models_to_start(AdapterModel* am);
DLL_PRIVATE void simbus_model_at_register(
    AdapterModel* am, Channel* channel, uint32_t model_uid);
DLL_PRIVATE void simbus_model_at_ready(
    AdapterModel* am, Channel* channel, uint32_t model_uid);
DLL_PRIVATE void simbus_model_at_exit(
    AdapterModel* am, Channel* channel, uint32_t model_uid);


#endif  // DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_PRIVATE_H_
