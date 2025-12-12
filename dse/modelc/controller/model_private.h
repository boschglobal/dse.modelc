// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_
#define DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_


#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/mcl.h>
#ifdef __linux__
#include <lua.h>
#endif

#define MI_RUNTIME_MCL_PATH     "runtime/mcl"
#define MI_RUNTIME_LUA_MCL_NAME "lua"


typedef struct ModelInstancePrivate {
    ControllerModel* controller_model;
    AdapterModel*    adapter_model;
    Controller*      controller;
#ifdef __linux__
    lua_State* lua_state;
#endif

    /* Runtime MCL Properties (built-in MCL's). */
    const char* mcl_name;
    MclCreate   mcl_create_func;
    MclDestroy  mcl_destroy_func;
} ModelInstancePrivate;


DLL_PRIVATE ModelDesc* mcl_builtin_model_create(ModelDesc* model);
DLL_PRIVATE int        mcl_builtin_model_step(
           ModelDesc* model, double* model_time, double stop_time);
DLL_PRIVATE void mcl_builtin_model_destroy(ModelDesc* model);


#endif  // DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_
