// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_
#define DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_


#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/mcl.h>
#include <lua.h>

#define MI_RUNTIME_MCL_PATH     "runtime/mcl"
#define MI_RUNTIME_LUA_MCL_NAME "lua"


typedef struct ModelInstancePrivate {
    ControllerModel* controller_model;
    AdapterModel*    adapter_model;
    Controller*      controller;
    lua_State*       lua_state;

    /* Runtime MCL Properties (built-in MCL's). */
    const char* mcl_name;
    MclCreate   mcl_create_func;
    MclDestroy  mcl_destroy_func;
} ModelInstancePrivate;


DLL_PRIVATE ModelDesc* mcl_builtin_model_create(ModelDesc* model);
DLL_PRIVATE int        mcl_builtin_model_step(
           ModelDesc* model, double* model_time, double stop_time);
DLL_PRIVATE void mcl_builtin_model_destroy(ModelDesc* model);

/* model_function.c */
DLL_PRIVATE const char* controller_get_signal_annotation(
    ModelFunctionChannel* mfc, const char* signal_name, const char* name);


#endif  // DSE_MODELC_CONTROLLER_MODEL_PRIVATE_H_
