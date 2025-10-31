// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_RUNTIME_H_
#define DSE_MODELC_RUNTIME_H_

#include <limits.h>
#include <dse/modelc/model.h>


/**
Runtime API
===========

The Runtime API provides methods for implementing a model Runtime/Importer
which can be used to load, configure and execute a model.

*/

typedef struct ChannelSpec {
    const char* name;
    const char* alias;
    /* Private data. */
    void* private;
} ChannelSpec;


typedef struct ModelDefinitionSpec {
    const char* name;
    const char* path;
    const char* file;
    char*       full_path; /* path + file */

    /* Reference to parsed YAML Documents. */
    void* doc;
    void* channels;
} ModelDefinitionSpec;


typedef struct ModelInstanceSpec {
    uint32_t            uid;
    char*               name;
    ModelDesc*          model_desc;
    ModelDefinitionSpec model_definition;

    /* Reference to parsed YAML Documents. */
    void* spec;
    void* yaml_doc_list;
    /* Private data of the specific Model Instance. */
    void* private;

    /* Reserved. */
    uint64_t __reserved__[8];
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
    /* The simulation is in a different location (i.e. not the CWD). */
    const char*        sim_path;
    /* Operational properties needed for loopback operation. */
    bool               mode_loopback;
    /* Operate a stacked simulation in Sequential CoSim mode. */
    bool               sequential_cosim;
    /* Reference to parsed YAML (stack:spec). */
    void*              spec;

    /* Reserved. */
#if defined(__x86_64__)
#if __SIZEOF_POINTER__ == 8
    uint64_t __reserved__[3];
#else
    uint32_t __reserved_4__;
    uint64_t __reserved__[3];
#endif
#elif defined(__i386__)
    uint32_t __reserved_4__;
    uint64_t __reserved__[3];
#endif
} SimulationSpec;


/* Loader/Runner API Definition */
typedef struct ModelCArguments {
    const char* transport;
    char*       uri;
    const char* host;
    uint32_t    port;
    double      timeout;
    uint8_t     log_level;
    double      step_size;
    double      end_time;
    uint32_t    uid;
    const char* name;
    const char* file;
    const char* path;
    /* Parsed YAML Files. */
    void*       yaml_doc_list;
    /* Allow detection of CLI provided arguments. */
    int         timeout_set_by_cli;
    int         log_level_set_by_cli;
    /* MStep "hidden" arguments. */
    uint32_t    steps;
    /* The simulation is in a different location (i.e. not the CWD). */
    const char* sim_path;

    /* Reserved. */
    uint64_t __reserved__[4];
} ModelCArguments;


/* modelc.c - Runtime Interface (i.e. ModelC.exe). */
DLL_PUBLIC int modelc_configure(ModelCArguments* args, SimulationSpec* sim);
DLL_PUBLIC ModelInstanceSpec* modelc_get_model_instance(
    SimulationSpec* sim, const char* name);
DLL_PUBLIC int  modelc_run(SimulationSpec* sim, bool run_async);
DLL_PUBLIC void modelc_exit(SimulationSpec* sim);
DLL_PUBLIC int  modelc_sync(SimulationSpec* sim);
DLL_PUBLIC void modelc_shutdown(SimulationSpec* sim);
DLL_PUBLIC int  modelc_model_create(
     SimulationSpec* sim, ModelInstanceSpec* mi, ModelVTable* model_vtable);


/* modelc_debug.c - Debug Interface. */
DLL_PUBLIC int  modelc_step(ModelInstanceSpec* mi, double step_size);
DLL_PUBLIC void modelc_destroy(ModelInstanceSpec* mi);


/* modelc_args.c - CLI argument parsing routines. */
DLL_PUBLIC void modelc_set_default_args(ModelCArguments* args,
    const char* model_instance_name, double step_size, double end_time);
DLL_PUBLIC void modelc_parse_arguments(
    ModelCArguments* args, int argc, char** argv, const char* doc_string);
DLL_PRIVATE void* modelc_find_stack(ModelCArguments* args);


/* signal.c - Signal Vector Interface. */
DLL_PUBLIC SignalVector* model_sv_create(ModelInstanceSpec* mi);
DLL_PUBLIC void          model_sv_destroy(SignalVector* sv);


/* ncodec.c - Stream Interface (for NCodec). */
DLL_PRIVATE void* model_sv_stream_create(SignalVector* sv, uint32_t idx);
DLL_PRIVATE void  model_sv_stream_destroy(void* stream);


/* model.c - Model Interface. */
DLL_PRIVATE ChannelSpec* model_build_channel_spec(
    ModelInstanceSpec* model_instance, const char* channel_name);
DLL_PRIVATE int model_configure_channel(ModelInstanceSpec* model_instance,
    const char* name, const char* function_name);


/* model_runtime.c - Runtime Interface (for operation in foreign systems). */
typedef struct RuntimeModelDesc RuntimeModelDesc;

#define MODEL_RUNTIME_CREATE_FUNC_NAME  "model_runtime_create"
#define MODEL_RUNTIME_STEP_FUNC_NAME    "model_runtime_step"
#define MODEL_RUNTIME_DESTROY_FUNC_NAME "model_runtime_destroy"

typedef void (*RuntimeModelSetEnv)(RuntimeModelDesc* m);

typedef struct RuntimeModelVTable {
    RuntimeModelSetEnv set_env;

    /* Reserved (space for 2 function pointers). */
    void* __reserved__[2];
} RuntimeModelVTable;


typedef struct RuntimeModelDesc {
    ModelDesc model;

    /*  Runtime properties. */
    struct {
        /* Used by Importer. */
        const char* runtime_model;

        /* Used by Importer/Runtime interface. */
        char*       sim_path;
        const char* model_name;  // Delimited list "a,b".
        const char* simulation_yaml;
        int         argc;
        char**      argv;
        void*       doc_list;

        /* Used by the Model Runtime. */
        uint8_t log_level;
        double  step_size;
        double  end_time;
        double  step_time_correction;
        bool    binary_signals_reset;

        /* Runtime Function Table - Importer provided functions. */
        RuntimeModelVTable vtable;
    } runtime;


    /* Reserved. */
#if defined(__x86_64__)
#if __SIZEOF_POINTER__ == 8
    uint64_t __reserved__[5];
#else
    uint32_t __reserved_4__[3];
    uint64_t __reserved__[5];
#endif
#elif defined(__i386__)
    uint32_t __reserved_4__[3];
    uint64_t __reserved__[5];
#endif
} RuntimeModelDesc;


DLL_PUBLIC RuntimeModelDesc* model_runtime_create(RuntimeModelDesc* model);
DLL_PUBLIC int               model_runtime_step(
                  RuntimeModelDesc* model, double* model_time, double stop_time);
DLL_PUBLIC void model_runtime_destroy(RuntimeModelDesc* model);


#endif  // DSE_MODELC_RUNTIME_H_
