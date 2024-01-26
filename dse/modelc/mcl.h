// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_MCL_H_
#define DSE_MODELC_MCL_H_

#include <dse/platform.h>
#include <dse/clib/util/yaml.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/modelc/runtime.h>


/**
Model Compatibility Layer
=========================

The Model Compatibility Layer (MCL) can be used to implement support for models
which were written according to different a modeling interface. This is done
by combining a Strategy and Adapter pattern to build an MCL which can load,
execute and exchange data with a the foreign model.


Component Diagram
-----------------
<div hidden>

```
@startuml mcl-component

title Compatibility Layer

component "ModelC" {
	component "MCL Model" as MCLmodel
}
component "MCL Lib" {
  interface "Strategy" as Sif
  interface "Adapter" as Aif
  component "MCL" as MCL
}
interface "Model Interface" as Mif
component "Model" as Model

MCLmodel --( Sif
MCLmodel --( Aif
Sif -- MCL
Aif -- MCL
MCL -( Mif
Mif - Model


center footer Dynamic Simulation Environment

@enduml
```

</div>

![](mcl-component.png)


*/


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
typedef int (*MclExecuteMethod)(ModelDesc* model, MclStrategyAction action);


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
    SignalVector*      mcl_channel_sv;
    MclStrategyDesc*   strategy;
    HashList           models; /* MclModelDesc */
} MclInstanceDesc;


/* mcl.c - Model Compatibility Library (MCL) interface.*/
DLL_PUBLIC void mcl_register_strategy(MclStrategyDesc* strategy);
DLL_PUBLIC void mcl_register_adapter(MclAdapterDesc* adapter);

DLL_PUBLIC int  mcl_load(ModelInstanceSpec* model_instance);
DLL_PUBLIC int  mcl_create(ModelInstanceSpec* model_instance);
DLL_PUBLIC void mcl_destroy(ModelInstanceSpec* model_instance);


#endif  // DSE_MODELC_MCL_H_
