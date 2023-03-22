// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_
#define DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_


#include <stdint.h>
#include <dse/platform.h>
#include <dse/modelc/adapter/adapter.h>


/* adapter.c */
DLL_PUBLIC Adapter* simbus_adapter_create(void* endpoint, double bus_step_size);
DLL_PUBLIC void     simbus_adapter_init_channel(
        AdapterModel* am, const char* channel_name, uint32_t expected_model_count);
DLL_PUBLIC void simbus_adapter_run(Adapter* adapter);


#endif  // DSE_MODELC_ADAPTER_SIMBUS_SIMBUS_H_
