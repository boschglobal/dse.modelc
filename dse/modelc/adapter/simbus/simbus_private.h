// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_PRIVATE_H_
#define DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_PRIVATE_H_


#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <dse_schemas/flatbuffers/simbus_channel_builder.h>
#include <dse_schemas/flatbuffers/simbus_notify_builder.h>
#include <dse/platform.h>
#include <dse/modelc/adapter/adapter.h>


#undef flatbuffers_identifier
#define flatbuffers_notify_identifier "SBNO"
#undef notify
#define notify(x) FLATBUFFERS_WRAP_NAMESPACE(dse_schemas_fbs_notify, x)


/* adapter.c */
DLL_PRIVATE uint32_t simbus_generate_uid_hash(const char* key);


/* handler.c */
DLL_PRIVATE void simbus_handle_notify_message(
    Adapter* adapter, notify(NotifyMessage_table_t) notify_message);


/* profile.c */
DLL_PRIVATE void simbus_profile_init(double bus_step_size);
DLL_PRIVATE void simbus_profile_accumulate_model_part(uint32_t model_uid,
    uint64_t me, uint64_t mp, uint64_t net, struct timespec ref_ts);
DLL_PRIVATE void simbus_profile_accumulate_cycle_total(
    uint64_t simbus_cycle_total_ns, struct timespec ref_ts);
DLL_PRIVATE void simbus_profile_print_benchmarks(void);
DLL_PRIVATE void simbus_profile_destroy(void);


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
