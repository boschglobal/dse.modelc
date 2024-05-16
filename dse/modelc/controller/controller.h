// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_CONTROLLER_CONTROLLER_H_
#define DSE_MODELC_CONTROLLER_CONTROLLER_H_


#include <stdint.h>
#include <stdbool.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/model.h>
#include <dse/platform.h>


typedef struct SignalTransform {
    struct LinearTransform {
        /* This transformation is disabled when factor = 0 (default). */
        double factor;
        double offset;
    } linear;
} SignalTransform;


typedef struct ModelFunctionChannel {
    const char*  channel_name;
    const char** signal_names;
    uint32_t     signal_count;

    /* Signal map (to adapter channel). */
    SignalMap* signal_map;
    uint32_t   signal_map_hash_code;

    /* Signal Value storage (in Vectors) and will be directly accessed by
       Model Functions. Only the configured type will be allocated. */
    double*   signal_value_double;
    void**    signal_value_binary;
    uint32_t* signal_value_binary_size;
    uint32_t* signal_value_binary_buffer_size;
    bool*     signal_value_binary_reset_called;

    /* Signal Transform; only allocated if transforms are present. */
    SignalTransform* signal_transform;
} ModelFunctionChannel;


typedef struct ModelFunction {
    const char* name;
    double      step_size;

    /* Collection of ModelFunctionChannel, Key is channel_name. */
    HashMap channels;
} ModelFunction;


typedef struct ControllerModel {
    /* Controller specific objects (placed in Model instance). */
    const char* model_dynlib_filename;

    /* Collection of ModelFunction, Key is Model Function name. */
    HashMap model_functions;

    /* Model interface vTable. */
    ModelVTable vtable;
} ControllerModel;


typedef struct Controller {
    bool            stop_request;
    /* Adapter/Endpoint objects. */
    Adapter*        adapter;
    /* Model configuration info: specific to a simulation. */
    SimulationSpec* simulation;
    HashMap         controller_models;  // index by model instance name.
} Controller;


/* controller.c */

/* These initialise the controller and load the Model lib. */
DLL_PRIVATE int controller_init(Endpoint* endpoint);
DLL_PRIVATE int controller_init_channel(ModelInstanceSpec* model_instance,
    const char* channel_name, const char** signal_name, uint32_t signal_count);

/* These are called indirectly from the Model, via _model_function_register()
   and model_configure_channel_*(). */
DLL_PRIVATE int controller_register_model_function(
    ModelInstanceSpec* model_instance, ModelFunction* model_function);
DLL_PRIVATE ModelFunction* controller_get_model_function(
    ModelInstanceSpec* model_instance, const char* model_function_name);

/* These control the operation of the Model. */
DLL_PRIVATE void controller_run(SimulationSpec* sim);
DLL_PRIVATE void controller_bus_ready(SimulationSpec* sim);
DLL_PRIVATE int  controller_step(SimulationSpec* sim);

DLL_PRIVATE void controller_stop(void);
DLL_PRIVATE void controller_dump_debug(void);
DLL_PRIVATE void controller_exit(SimulationSpec* sim);


/* loader.c */
DLL_PRIVATE int controller_load_models(SimulationSpec* sim);


/* step.c */
DLL_PRIVATE int step_model(ModelInstanceSpec* mi, double* model_time);
DLL_PRIVATE int sim_step_models(SimulationSpec* sim, double* model_time);


/* model.c */
DLL_PUBLIC void model_function_destroy(ModelFunction* model_function);


/* transform.c */
DLL_PRIVATE void controller_transform_to_model(
    ModelFunctionChannel* mfc, SignalMap* sm);
DLL_PRIVATE void controller_transform_from_model(
    ModelFunctionChannel* mfc, SignalMap* sm);


#endif  // DSE_MODELC_CONTROLLER_CONTROLLER_H_
