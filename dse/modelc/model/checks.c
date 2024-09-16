// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/modelc/gateway.h>
#include <dse/modelc/mcl.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>


__attribute__((unused)) static void __compile_time_checks(void)
{
    // Compile-time type size check. Get actual size with:
    // char(*___)[sizeof(ModelGatewayDesc)] = 1;

    // char(*___)[sizeof(MclDesc)] = 1;
    // char(*___)[sizeof(MarshalSignalMap)] = 1;

    // Compile-time type size check. Get actual size with:
    // char(*___)[sizeof(SimulationSpec)] = 1;
    // char(*___)[sizeof(ModelInstanceSpec)] = 1;
    // char(*___)[sizeof(ModelDesc)] = 1;
    // char(*___)[sizeof(ModelCArguments)] = 1;
    // char(*___)[sizeof(RuntimeModelDesc)] = 1;

    // char (*___)[sizeof(SignalVector)] = 1;

#if defined(__x86_64__)
    #if __SIZEOF_POINTER__ == 8
    _Static_assert(sizeof(ModelGatewayDesc) == 80, "Compatibility FAIL!");
    _Static_assert(sizeof(MclDesc) == 272, "Compatibility FAIL!");
    _Static_assert(sizeof(MarshalSignalMap) == 56, "Compatibility FAIL!");
    _Static_assert(sizeof(SimulationSpec) == 104, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelInstanceSpec) == 160, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelDesc) == 112, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelCArguments) == 160, "Compatibility FAIL!");
    _Static_assert(sizeof(RuntimeModelDesc) == 272, "Compatibility FAIL!");
    _Static_assert(sizeof(SignalVector) == 256, "Compatibility FAIL!");
    #else
    _Static_assert(sizeof(MclDesc) == 176, "Compatibility FAIL!");
    _Static_assert(sizeof(MarshalSignalMap) == 28, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelGatewayDesc) == 64, "Compatibility FAIL!");
    _Static_assert(sizeof(SimulationSpec) == 88, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelInstanceSpec) == 112, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelDesc) == 72, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelCArguments) == 120, "Compatibility FAIL!");
    _Static_assert(sizeof(RuntimeModelDesc) == 200, "Compatibility FAIL!");
    _Static_assert(sizeof(SignalVector) == 160, "Compatibility FAIL!");
    #endif
#elif defined(__i386__)
    _Static_assert(sizeof(ModelGatewayDesc) == 60, "Compatibility FAIL!");
    _Static_assert(sizeof(MclDesc) == 176, "Compatibility FAIL!");
    _Static_assert(sizeof(MarshalSignalMap) == 28, "Compatibility FAIL!");
    _Static_assert(sizeof(SimulationSpec) == 80, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelInstanceSpec) == 112, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelDesc) == 72, "Compatibility FAIL!");
    _Static_assert(sizeof(ModelCArguments) == 112, "Compatibility FAIL!");
    _Static_assert(sizeof(RuntimeModelDesc) == 196, "Compatibility FAIL!");
    _Static_assert(sizeof(SignalVector) == 160, "Compatibility FAIL!");
#endif
}
