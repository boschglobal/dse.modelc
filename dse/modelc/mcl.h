// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MCL_H_
#define DSE_MODELC_MCL_H_

/*
DSE Model Compatibility Layer (MCL)
===================================

The Model Compatibility Layer can be used to implement support for models which
were written according to different modeling interface.
*/

#include <dse/modelc/model.h>
#include <dse/platform.h>
#include <dse/clib/collections/hashlist.h>


#define MCL_SETUP_FUNC     mcl_setup
#define MCL_SETUP_FUNC_STR "mcl_setup"


typedef enum MclStrategyAction {
    MCL_STRATEGY_ACTION_NONE = 0,
    /* These strategy actions are implemented by a MCL Adapter. */
    MCL_STRATEGY_ACTION_LOAD,
    MCL_STRATEGY_ACTION_INIT,
    MCL_STRATEGY_ACTION_STEP,
    MCL_STRATEGY_ACTION_UNLOAD,
    /* These strategy actions are implemented by an MCL Strategy. */
    MCL_STRATEGY_ACTION_MARSHALL_OUT,
    MCL_STRATEGY_ACTION_MARSHALL_IN,
    /* Count of all actions. */
    __MCL_STRATEGY_ACTION_COUNT__
} MclStrategyAction;

typedef struct MclInstanceDesc MclInstanceDesc;
typedef struct MclModelDesc    MclModelDesc;


/* MCL Strategy methods (generic model provided). */
typedef int (*MclExecuteMethod)(MclStrategyAction action);


/* MCL Strategy handler functions. */
typedef int (*MclExecuteHandler)(
    MclInstanceDesc* mcl_instance, MclStrategyAction action);
typedef int (*MclMarshallOutHandler)(MclInstanceDesc* mcl_instance);
typedef int (*MclMarshallInHandler)(MclInstanceDesc* mcl_instance);


/* MCL Adapter handler functions. */
typedef int (*MclRegisterHandler)(void);
typedef int (*MclLoadHandler)(
    MclModelDesc* model_desc, ModelInstanceSpec* model_instance);
typedef int (*MclInitHandler)(
    MclModelDesc* model_desc, ModelInstanceSpec* model_instance);
typedef int (*MclStepHandler)(
    MclModelDesc* model_desc, double* model_time, double stop_time);
typedef int (*MclUnloadHandler)(MclModelDesc* model_desc);


typedef struct MclStrategyDesc {
    const char*           name;
    /* Call data: do_step(). Set before call to execute(ACTION_STEP). */
    double                model_time;
    double                stop_time;
    /* Strategy methods (generic model provided). */
    MclExecuteMethod      execute;
    /* Strategy handlers (strategy implementation provided). */
    MclExecuteHandler     execute_func;
    MclMarshallOutHandler marshall_out_func;
    MclMarshallInHandler  marshall_in_func;
    /* Reference to the containing MCL Instance. */
    MclInstanceDesc*      mcl_instance;
} MclStrategyDesc;


typedef struct MclAdapterDesc {
    const char*      name;
    /* Adapter methods. */
    MclLoadHandler   load_func;
    MclInitHandler   init_func;
    MclStepHandler   step_func;
    MclUnloadHandler unload_func;
} MclAdapterDesc;


typedef struct MclModelDesc {
    const char*     name;
    /* Model properties. */
    YamlNode*       model_doc;
    char*           path;
    void*           handle;
    /* Strategy properties (use to ensure data correctness/concurrency). */
    double*         vector_double;
    void**          vector_binary; /* Pointer to vector of void*. */
    /* Supporting time progression within an MCL (when step_size != 0). */
    double          model_time;
    double          model_time_correction;
    double          step_size;
    /* Compatibility Layer for this Model. */
    MclAdapterDesc* adapter;
    /* Private data of the specific Model. */
    void* private;
} MclModelDesc;


typedef struct MclInstanceDesc {
    ModelInstanceSpec* model_instance;
    /* Instance Properties. */
    ModelChannelDesc*  channel;
    MclStrategyDesc*   strategy;
    HashList           models; /* MclModelDesc */
} MclInstanceDesc;

/* This portion of the API is implemented by an MCL. */
DLL_PUBLIC int MCL_SETUP_FUNC(void);


/* mcl.c - Model Compatibility Library (MCL) interface.*/
DLL_PUBLIC void mcl_register_strategy(MclStrategyDesc* strategy);
DLL_PUBLIC void mcl_register_adapter(MclAdapterDesc* adapter);

DLL_PUBLIC int  mcl_load(ModelInstanceSpec* model_instance);
DLL_PUBLIC int  mcl_create(ModelInstanceSpec* model_instance);
DLL_PUBLIC void mcl_destroy(ModelInstanceSpec* model_instance);


#endif  // DSE_MODELC_MCL_H_
