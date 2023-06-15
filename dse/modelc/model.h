// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MODEL_H_
#define DSE_MODELC_MODEL_H_

/*
DSE Model
=========

Model interface of the Dynamic Simulation Environment (DSE).
*/

#include <stdint.h>
#include <stdbool.h>
#include <dse/platform.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/yaml.h>

#define __MODELC_ERROR_OFFSET (2000)

#define MODEL_SETUP_FUNC     model_setup
#define MODEL_SETUP_FUNC_STR "model_setup"

#define MODEL_EXIT_FUNC      model_exit
#define MODEL_EXIT_FUNC_STR  "model_exit"


typedef struct ModelDefinitionSpec {
    const char* name;
    /* Path to the Model Package (i.e. where model.yaml is found). */
    const char* path;
    /* Path to the Model Lib, relative to path (above). */
    const char* file;
    /* Combined path and file for the Model Lib. */
    char*       full_path;
    /* Reference to parsed YAML Documents. */
    YamlNode*   doc;
    YamlNode*   channels;
} ModelDefinitionSpec;


typedef struct ModelInstanceSpec {
    uint32_t            uid;
    char*               name;
    ModelDefinitionSpec model_definition;
    /* Reference to parsed YAML Documents. */
    YamlNode*           spec;
    YamlNode*           propagators;
    YamlDocList*        yaml_doc_list;
    /* Private data of the specific Model Instance. */
    void* private;
} ModelInstanceSpec;


typedef struct SimulationSpec {
    /* Transport. */
    const char*        transport;
    char*              uri;
    uint32_t           uid;
    double             timeout;
    /* Simulation. */
    double             step_size;
    double             end_time;
    /* Model Instances, list, last entry all values set NULL (EOL detect). */
    ModelInstanceSpec* instance_list;
} SimulationSpec;


typedef struct ModelChannelDesc {
    const char*  name;
    const char*  function_name;
    /* Reference to the parsed signal names. */
    const char** signal_names;
    uint32_t     signal_count;
    /* Indicate if this Channel is connected to a Propagator Model. */
    bool         propagator_source_channel;
    bool         propagator_target_channel;
    /* Allocated vector table (one only depending on type). */
    double*      vector_double;
    void**       vector_binary;
    /* Additional vector tables supporting vector_binary. */
    uint32_t*    vector_binary_size;        /* Size of binary object. */
    uint32_t*    vector_binary_buffer_size; /* Size of allocated buffer. */
} ModelChannelDesc;


typedef struct ChannelSpec {
    const char* name;
    const char* alias;
    /* Private data. */
    void* private;
} ChannelSpec;


/*
Signal Vector API Definition
============================

Object representation of Signal Vectors which encapsulate data and behaviour.

Primary use in consumer APIs (i.e. Model Gateway API).
*/

typedef struct SignalVector SignalVector;

typedef int (*BinarySignalAppendFunc)(
    SignalVector* sv, uint32_t index, void* data, uint32_t len);
typedef int (*BinarySignalResetFunc)(SignalVector* sv, uint32_t index);
typedef int (*BinarySignalReleaseFunc)(SignalVector* sv, uint32_t index);
typedef const char* (*SignalAnnotationGetFunc)(
    SignalVector* sv, uint32_t index, const char* name);

typedef struct SignalVector {
    const char*  name;
    const char*  function_name;
    bool         is_binary;
    /* Vector representation of Signals (each with _count_ elements). */
    uint32_t     count;
    const char** signal; /* Signal name. */
    union {              /* Signal value. */
        struct {
            double* scalar;
        };
        struct {
            void**       binary;
            uint32_t*    length;      /* Length of binary object. */
            uint32_t*    buffer_size; /* Size of allocated buffer. */
            const char** mime_type;
        };
    };
    /* Helper functions. */
    BinarySignalAppendFunc  append;
    BinarySignalResetFunc   reset;
    BinarySignalReleaseFunc release;
    SignalAnnotationGetFunc annotation;
    /* Reference data. */
    ModelInstanceSpec*      mi;
} SignalVector;


/*
Model API Definition
====================

This portion of the API is implemented by a Model.


Example
-------

    // my_model.c
    #include <dse/modelc/model/model.h>

    static const char* signal_name[] = {
        "foo",
        "bar",
    };
    static uint32_t signal_count = 2;
    static double* signal_value;

    int do_step(double *model_time, double stop_time)
    {
        signal_value[0] += 1.2;
        *model_time = stop_time;

        return 0;
    }

    int model_setup(ModelInstanceSpec* model_instance)
    {
        ModelFunction* _mf;
        _mf = model_function_register("Example", 0.001, do_step);
        signal_value = model_configure_channel_double(
                _mf, "TEST", signal_name, signal_count);

        return 0;
    }
*/

typedef int (*ModelSetupHandler)(ModelInstanceSpec* model_instance);
typedef int (*ModelDoStepHandler)(double* model_time, double stop_time);
typedef int (*ModelExitHandler)(ModelInstanceSpec* model_instance);

DLL_PUBLIC int MODEL_SETUP_FUNC(ModelInstanceSpec* model_instance);


/*
Loader/Runner API Definition
============================

This portion of the API is implemented by a Model Loader/Runner (i.e. ModelC).
*/

typedef struct ModelCArguments {
    const char*  transport;
    char*        uri;
    const char*  host;
    uint32_t     port;
    double       timeout;
    uint8_t      log_level;
    double       step_size;
    double       end_time;
    uint32_t     uid;
    const char*  name;
    const char*  file;
    const char*  path;
    /* Parsed YAML Files. */
    YamlDocList* yaml_doc_list;
    /* Allow detection of CLI provided arguments. */
    int          timeout_set_by_cli;
    int          log_level_set_by_cli;
} ModelCArguments;


/* model.c - Model Interface. */
DLL_PUBLIC int model_function_register(ModelInstanceSpec* model_instance,
    const char* model_function_name, double step_size,
    ModelDoStepHandler do_step_handler);
DLL_PUBLIC int model_configure_channel(
    ModelInstanceSpec* model_instance, ModelChannelDesc* channel_desc);
DLL_PUBLIC ChannelSpec* model_build_channel_spec(
    ModelInstanceSpec* model_instance, const char* channel_name);


/* signal.c - Signal Vector Interface. */
DLL_PUBLIC SignalVector* model_sv_create(ModelInstanceSpec* mi);
DLL_PUBLIC void          model_sv_destroy(SignalVector* sv);


/* modelc.c - Runtime Interface (i.e. ModelC.exe). */
DLL_PUBLIC int  modelc_configure(ModelCArguments* args, SimulationSpec* sim);
DLL_PUBLIC ModelInstanceSpec* modelc_get_model_instance(
    SimulationSpec* sim, const char* name);
DLL_PUBLIC int  modelc_run(SimulationSpec* sim, bool run_async);
DLL_PUBLIC void modelc_exit(SimulationSpec* sim);
DLL_PUBLIC int  modelc_sync(SimulationSpec* sim);
DLL_PUBLIC void modelc_shutdown(void);


/* modelc_debug.c - Debug Interface. */
DLL_PUBLIC int modelc_step(ModelInstanceSpec* model_instance, double step_size);


/* modelc_args.c - CLI argument parsing routines. */
DLL_PUBLIC void modelc_set_default_args(ModelCArguments* args,
    const char* model_instance_name, double step_size, double end_time);
DLL_PUBLIC void modelc_parse_arguments(
    ModelCArguments* args, int argc, char** argv, const char* doc_string);


#endif  // DSE_MODELC_MODEL_H_
