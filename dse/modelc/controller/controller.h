// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_CONTROLLER_CONTROLLER_H_
#define DSE_MODELC_CONTROLLER_CONTROLLER_H_


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/model.h>
#include <dse/modelc/model/lua.h>
#include <dse/platform.h>

typedef struct SimbusChannel SimbusChannel;


typedef struct SignalTransform {
    struct LinearTransform {
        /* This transformation is disabled when factor = 0 (default). */
        double factor;
        double offset;
    } linear;
    struct FunctionTransform {
        struct ModelFT {
            const char* lua_script;
            int32_t     ref;
        } model;
        struct VectorFT {
            const char* lua_script;
            int32_t     ref;
        } vector;
    } function;
    struct TimingTransform {
        float interval;
        float phase;
    } timing;
} SignalTransform;


typedef struct FlatSyncMap {
    bool           enabled;
    bool           capable;
    uint32_t       sync_count;
    uint32_t*      simbus_indices;
    double*        simbus_scalar;
    SimbusChannel* simbus_channel;
    double*        shadow;
    double*        output_shadow;
    bool           output_shadow_initialized;
} FlatSyncMap;

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
    double*   signal_value_double_shadow;
    void**    signal_value_binary;
    uint32_t* signal_value_binary_size;
    uint32_t* signal_value_binary_buffer_size;
    bool*     signal_value_binary_reset_called;

    /* Flat scalar sync map used to evaluate a dense scatter/gather path.
       This sits next to the legacy loop-based copy code so the fast path can be
       enabled/disabled for benchmarking without changing binary handling. */
    FlatSyncMap scalar_sync;

    /* Signal Transform; only allocated if transforms are present. */
    SignalTransform* signal_transform;

    /* Signal Annotation reference (to YAML nodes). */
    void** signal_annotation;
} ModelFunctionChannel;

typedef struct ModelFunctionChannelIndexItem {
    const char*           name;
    ModelFunctionChannel* mfc;
} ModelFunctionChannelIndexItem;

static __inline__ int controller_name2mfc_compar(const void* a, const void* b)
{
    const ModelFunctionChannelIndexItem* x = a;
    const ModelFunctionChannelIndexItem* y = b;
    return strcmp(x->name, y->name);
}

typedef struct ModelFunction {
    const char* name;
    double      step_size;

    /* Collection of ModelFunctionChannel, Key is channel_name. */
    Vector channels;  // vector{ModelFunctionChannelIndexItem}
} ModelFunction;

typedef struct ModelFunctionIndexItem {
    const char*    name;
    ModelFunction* mf;
} ModelFunctionIndexItem;

static __inline__ int controller_name2mf_compar(const void* a, const void* b)
{
    const ModelFunctionIndexItem* x = a;
    const ModelFunctionIndexItem* y = b;
    return strcmp(x->name, y->name);
}

typedef struct ControllerModel {
    /* Controller specific objects (placed in Model instance). */
    const char* model_dynlib_filename;
    void*       handle;  // Handle for loaded model.

    /* Collection of ModelFunction, Key is Model Function name. */
    Vector model_functions;  // vector{ModelFunctionIndexItem}

    /* Model interface vTable. */
    ModelVTable vtable;
} ControllerModel;


typedef struct Controller {
    bool            stop_request;
    /* Adapter/Endpoint objects. */
    Adapter*        adapter;
    /* Model configuration info: specific to a simulation. */
    SimulationSpec* simulation;
} Controller;


typedef enum ControllerMarshalDir {
    MARSHAL_ADAPTER2MODEL,
    MARSHAL_MODEL2ADAPTER,
    MARSHAL_ADAPTER2MODEL_SCALAR_ONLY,
    MARSHAL_MODEL2ADAPTER_SCALAR_ONLY,
    MARSHAL_MODEL2ADAPTER_BINARY_ONLY,
} ControllerMarshalDir;


typedef struct ControllerMarshalSpec {
    ControllerMarshalDir dir;
    ModelInstanceSpec*   mi;
} ControllerMarshalSpec;


/* controller.c */

/* These initialise the controller and load the Model lib. */
DLL_PRIVATE int controller_init(Endpoint* endpoint, SimulationSpec* sim);
DLL_PRIVATE int controller_init_channel(ModelInstanceSpec* model_instance,
    const char* channel_name, const char** signal_name, uint32_t signal_count,
    ModelFunctionChannel* mfc);
DLL_PRIVATE Controller* controller_object_ref(SimulationSpec* sim);

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
DLL_PRIVATE int  controller_step_phased(SimulationSpec* sim);

DLL_PRIVATE void controller_stop(SimulationSpec* sim);
DLL_PRIVATE void controller_dump_debug(SimulationSpec* sim);
DLL_PRIVATE void controller_exit(SimulationSpec* sim);

/* Additional marshal operations that are called in special cases. */
DLL_PRIVATE void marshal_model(ModelInstanceSpec* mi, ControllerMarshalDir dir);

/* loader.c */
DLL_PRIVATE int controller_load_models(SimulationSpec* sim);


/* step.c */
DLL_PRIVATE int step_model(ModelInstanceSpec* mi, double* model_time);
DLL_PRIVATE int sim_step_models(SimulationSpec* sim, double* model_time);


/* model.c */
DLL_PUBLIC void model_function_destroy(ModelFunction* model_function);


/* transform.c */
DLL_PRIVATE void model_function_channel_build_flat_sync(
    ModelFunctionChannel* mfc, SignalMap* sm);
DLL_PRIVATE void model_function_channel_free_flat_sync(
    ModelFunctionChannel* mfc);
DLL_PRIVATE void controller_transform_to_model(
    ModelFunctionChannel* mfc, SignalMap* sm, lua_State* L);
DLL_PRIVATE void controller_transform_from_model(
    ModelFunctionChannel* mfc, SignalMap* sm, lua_State* L);


#endif  // DSE_MODELC_CONTROLLER_CONTROLLER_H_
