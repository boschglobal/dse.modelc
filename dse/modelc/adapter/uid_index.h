// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_UID_INDEX_H_
#define DSE_MODELC_ADAPTER_UID_INDEX_H_

#include <stdbool.h>
#include <stdint.h>
#include <dse/modelc/adapter/adapter.h>


DLL_PRIVATE bool signal_value_uid_index_insert(
    SignalValueUidIndex** index, uint32_t uid, SignalValue* signal);
DLL_PRIVATE SignalValue* signal_value_uid_index_find(
    const SignalValueUidIndex* index, uint32_t uid);
DLL_PRIVATE void signal_value_uid_index_destroy(SignalValueUidIndex* index);


#endif  // DSE_MODELC_ADAPTER_UID_INDEX_H_
